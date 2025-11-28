"""
SPDX-FileCopyrightText: 2025 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2
"""

import datetime
import pathlib

import pytest

from constellation.core.configuration import Configuration
from constellation.core.controller_configuration import (
    ConfigParseError,
    ConfigValidationError,
    ControllerConfiguration,
    FileType,
)


def test_ctrl_config_creation():
    global_dict = {"level": "global", "int": 42, "dict": {"level": "global", "int": 42}}
    type_dict = {"level": "type", "dict": {"level": "type"}}
    satellite_dict = {"level": "satellite", "dict": {"level": "satellite"}}

    config = ControllerConfiguration()
    config.set_global_configuration(global_dict)
    config.add_type_configuration("Dummy", type_dict)
    config.add_satellite_configuration("Dummy.Added", satellite_dict)

    # Check global config
    global_config = config.get_global_configuration()
    assert global_config.get("level") == "global"
    assert global_config.get("int") == 42
    assert global_config.get_section("dict").get("level") == "global"
    assert global_config.get_section("dict").get("int") == 42

    # Check that type config is available
    assert config.has_type_configuration("dummY")
    assert not config.has_satellite_configuration("dummy2")

    # Check type config
    type_config = config.get_type_configuration("DUMMY")
    assert type_config.get("level") == "type"
    assert type_config.get("int") == 42
    assert type_config.get_section("dict").get("level") == "type"
    assert type_config.get_section("dict").get("int") == 42

    # Check that satellite config is available
    assert config.has_satellite_configuration("dummY.addeD")
    assert not config.has_satellite_configuration("dummy2")

    # Check satellite config
    satellite_config = config.get_satellite_configuration("DUMMY.ADDED")
    assert satellite_config.get("level") == "satellite"
    assert satellite_config.get("int") == 42
    assert satellite_config.get_section("dict").get("level") == "satellite"
    assert satellite_config.get_section("dict").get("int") == 42


def test_ctrl_config_invalid_creation():
    global_dict = {"invalid_type": object()}
    config = ControllerConfiguration()
    with pytest.raises(ConfigParseError) as excinfo:
        config.set_global_configuration(global_dict)
    assert str(excinfo.value) == "Could not parse configuration: value of key `invalid_type` not scalar"


def test_ctrl_config_merge_levels():
    type_dict = {"int": 0}
    satellite_dict = {"int": 10}

    config = ControllerConfiguration()
    config.add_type_configuration("Dummy", type_dict)
    config.add_satellite_configuration("Dummy.Added", satellite_dict)

    type_dict_updated = {"int": 1}
    satellite_dict_updated = {"int": 11}

    config.set_global_configuration(Configuration({}))
    config.add_type_configuration("Dummy", Configuration(type_dict_updated))
    config.add_satellite_configuration("Dummy.Added", Configuration(satellite_dict_updated))

    assert config.get_type_configuration("Dummy").get("int") == 1
    assert config.get_satellite_configuration("Dummy.Added").get("int") == 11


def test_ctrl_config_merge_levels_type_mismatch():
    type_dict = {"dict": {"int": 0}}
    satellite_dict = {"dict": {"int": {"a": 1}}}

    config = ControllerConfiguration()
    config.add_type_configuration("Dummy", type_dict)
    config.add_satellite_configuration("Dummy.Added", satellite_dict)

    with pytest.raises(ConfigValidationError) as excinfo:
        config.get_satellite_configuration("Dummy.Added")
    assert (
        str(excinfo.value) == "Could not parse configuration: error validating configuration: "
        "value of key `dict.int` has mismatched types when merging defaults"
    )


def test_ctrl_config_parse_empty_yaml():
    ControllerConfiguration.from_string("", FileType.YAML)


def test_ctrl_config_parse_invalid_yaml():
    config = "a: b: c"
    with pytest.raises(ConfigParseError):
        ControllerConfiguration.from_string(config, FileType.YAML)


def test_ctrl_config_parse_invalid_yaml_non_map_root_node():
    config = "root_node"
    with pytest.raises(ConfigParseError) as excinfo:
        ControllerConfiguration.from_string(config, FileType.YAML)
    assert str(excinfo.value) == "Could not parse configuration: expected map as root node"


def test_ctrl_config_parse_invalid_yaml_type_key_not_string():
    config = "1:\n  11: 12\n2:\n  21: 22"
    with pytest.raises(ConfigParseError) as excinfo:
        ControllerConfiguration.from_string(config, FileType.YAML)
    assert str(excinfo.value) == "Could not parse configuration: error while parsing key `1`: key not a string"


def test_ctrl_config_parse_invalid_yaml_name_key_not_string():
    config = "'1':\n  11: 12\n'2':\n  21: 22"
    with pytest.raises(ConfigParseError) as excinfo:
        ControllerConfiguration.from_string(config, FileType.YAML)
    assert str(excinfo.value) == "Could not parse configuration: error while parsing key `1.11`: key not a string"


def test_ctrl_config_parse_empty_toml():
    ControllerConfiguration.from_string("", FileType.TOML)


def test_ctrl_config_parse_invalid_toml():
    config = "a: b: c"
    with pytest.raises(ConfigParseError):
        ControllerConfiguration.from_string(config, FileType.TOML)


def test_ctrl_config_invalid_type_node_not_dict():
    config = "key = 0"
    with pytest.raises(ConfigParseError) as excinfo:
        ControllerConfiguration.from_string(config, FileType.TOML)
    assert (
        str(excinfo.value) == "Could not parse configuration: error while parsing value of key `key`: "
        "expected a dictionary at type level"
    )


def test_ctrl_config_invalid_two_global_default_configs():
    config = "[_default]\nkey = 0\n[_DEFAULT]\nkey = 1"
    with pytest.raises(ConfigParseError) as excinfo:
        ControllerConfiguration.from_string(config, FileType.TOML)
    assert str(excinfo.value) == "Could not parse configuration: error while parsing key `_default`: key defined twice"


def test_ctrl_config_invalid_name_node_not_dict():
    config = "[type]\nname = 0"
    with pytest.raises(ConfigParseError) as excinfo:
        ControllerConfiguration.from_string(config, FileType.TOML)
    assert (
        str(excinfo.value) == "Could not parse configuration: error while parsing value of key `type.name`: "
        "expected a dictionary at satellite level"
    )


def test_ctrl_config_invalid_two_type_default_configs():
    config = "[type._default]\nkey = 0\n[TYPE._DEFAULT]\nkey = 1"
    with pytest.raises(ConfigParseError) as excinfo:
        ControllerConfiguration.from_string(config, FileType.TOML)
    assert str(excinfo.value) == "Could not parse configuration: error while parsing key `type._default`: key defined twice"


def test_ctrl_config_invalid_two_satellite_configs():
    config = "[type.name]\nkey = 0\n[type.NAME]\nkey = 1"
    with pytest.raises(ConfigParseError) as excinfo:
        ControllerConfiguration.from_string(config, FileType.TOML)
    assert str(excinfo.value) == "Could not parse configuration: error while parsing key `type.name`: key defined twice"


def _timetz(*args) -> datetime.time:
    return datetime.datetime(2025, 12, 7, *args).astimezone(datetime.UTC).timetz()


def test_ctrl_config_valid_toml():
    test_files_path = pathlib.Path(__file__).parent / "test_files"
    test_file = test_files_path / "good_config.toml"
    config = ControllerConfiguration.from_path(test_file)

    # Global
    global_config = config.get_satellite_configuration("NotA.Satellite")
    assert global_config.get("bool") == True  # noqa: E712
    assert global_config.get("int") == -42
    assert global_config.get("float") == 3.14
    assert global_config.get("string") == "global"
    assert global_config.get("time", return_type=lambda x: x.timetz()) == _timetz(12, 34, 56)
    assert global_config.get("datetime") == datetime.datetime(2025, 12, 7, 12, 34, 56, tzinfo=datetime.UTC)
    assert global_config.get("array_bool") == [True, False, False, True]
    assert global_config.get("array_int") == [1, 2, 3]
    assert global_config.get("array_float") == [0.5, 1.0]
    assert global_config.get("array_string") == ["global1", "global2"]
    assert global_config.get_array("array_time", element_type=lambda x: x.timetz()) == [
        _timetz(12, 00),
        _timetz(13, 00),
    ]
    assert global_config.get("empty_array") == []
    assert global_config.get_section("dict").get_section("subdict").get("key") == -1
    assert len(global_config.get_section("empty_dict")) == 0

    # Global + Type
    type_config = config.get_satellite_configuration("Dummy.NotASatellite")
    assert type_config.get("bool") == True  # noqa: E712
    assert type_config.get("type") == "Dummy"
    assert type_config.get("string") == "type"
    assert type_config.get_section("dict").get_section("subdict").get("key") == 0

    # Global + Type + Satellite
    d1_config = config.get_satellite_configuration("Dummy.D1")
    assert d1_config.get("bool") == True  # noqa: E712
    assert d1_config.get("type") == "Dummy"
    assert d1_config.get("string") == "D1"
    assert d1_config.get_section("dict").get_section("subdict").get("key") == 1

    # Case-insensitivity check
    d2_config = config.get_satellite_configuration("Dummy.D2")
    assert d2_config.get("bool") == True  # noqa: E712
    assert d2_config.get("type") == "Dummy"
    assert d2_config.get("string") == "D2"
    assert d2_config.get_section("dict").get_section("subdict").get("key") == 2

    # Check that empty satellite configurations are registered
    assert config.has_satellite_configuration("Dummy3.D3")


def test_ctrl_config_valid_yaml():
    test_files_path = pathlib.Path(__file__).parent / "test_files"
    test_file = test_files_path / "good_config.yaml"
    config = ControllerConfiguration.from_path(test_file)

    # Global
    global_config = config.get_satellite_configuration("NotA.Satellite")
    assert global_config.get("bool") == True  # noqa: E712
    assert global_config.get("int") == -42
    assert global_config.get("float") == 3.14
    assert global_config.get("string") == "global"
    assert global_config.get("array_bool") == [True, False, False, True]
    assert global_config.get("array_int") == [1, 2, 3]
    assert global_config.get("array_float") == [0.5, 1.0]
    assert global_config.get("array_string") == ["global1", "global2"]
    assert global_config.get("empty_array") == []
    assert global_config.get_section("dict").get_section("subdict").get("key") == -1
    assert len(global_config.get_section("empty_dict")) == 0

    # Global + Type
    type_config = config.get_satellite_configuration("Dummy.NotASatellite")
    assert type_config.get("bool") == True  # noqa: E712
    assert type_config.get("type") == "Dummy"
    assert type_config.get("string") == "type"
    assert type_config.get_section("dict").get_section("subdict").get("key") == 0

    # Global + Type + Satellite
    d1_config = config.get_satellite_configuration("Dummy.D1")
    assert d1_config.get("bool") == True  # noqa: E712
    assert d1_config.get("type") == "Dummy"
    assert d1_config.get("string") == "D1"
    assert d1_config.get_section("dict").get_section("subdict").get("key") == 1

    # Case-insensitivity check
    d2_config = config.get_satellite_configuration("Dummy.D2")
    assert d2_config.get("bool") == True  # noqa: E712
    assert d2_config.get("type") == "Dummy"
    assert d2_config.get("string") == "D2"
    assert d2_config.get_section("dict").get_section("subdict").get("key") == 2

    # Check that empty satellite configurations are registered
    assert config.has_satellite_configuration("Dummy3.D3")


def test_ctrl_config_str():
    global_dict = {"level": "global"}
    type_dict = {"level": "type"}
    satellite_1_dict = {"level": "satellite"}
    satellite_2_dict = {"level": "satellite"}
    config = ControllerConfiguration()
    config.set_global_configuration(global_dict)
    config.add_type_configuration("dummy", type_dict)
    config.add_satellite_configuration("dummy.one", satellite_1_dict)
    config.add_satellite_configuration("dummy2.two", satellite_2_dict)
    single_dict = {
        "_default": global_dict,
        "dummy": {"_default": type_dict, "one": satellite_1_dict},
        "dummy2": {"two": satellite_2_dict},
    }
    assert config._as_single_dict() == single_dict
    assert str(config) == str(single_dict)
