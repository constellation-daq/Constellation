"""
SPDX-FileCopyrightText: 2025 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2
"""

import datetime
import enum
import os
import pathlib

import pytest

from constellation.core.configuration import (
    Configuration,
    ConfigurationGroup,
    InvalidTypeError,
    InvalidUpdateError,
    InvalidValueError,
    MissingKeyError,
)


class DummyEnum(enum.Enum):
    A = enum.auto()
    B = enum.auto()


def test_configuration_has_and_count():
    config = Configuration(
        {
            "output_active": True,
            "fixed_voltage": 5.0,
            "fixed_current": 0.1,
        }
    )
    assert config.has("output_active")
    assert "OUTPUT_ACTIVE" in config
    assert not config.has("output_disabled")
    assert "output_disabled" not in config
    assert config.count(["fixed_voltage", "fixed_current"]) == 2
    assert config.count(["FIXED_VOLTAGE", "FIXED_CURRENT"]) == 2
    assert config.count(["output_disabled"]) == 0
    assert len(config) == 3
    with pytest.raises(ValueError) as excinfo:
        config.count([])
    assert str(excinfo.value) == "list of keys to count cannot be empty"


def test_configuration_get():
    bool_v = False
    int_v = 16
    str_v = "hello world"
    enum_v = DummyEnum.A
    config = Configuration({"bool": bool_v, "int": int_v, "str": str_v, "enum": enum_v.name})
    # get without arguments
    assert config.get("bool") == bool_v
    assert config.get("int") == int_v
    assert config.get("str") == str_v
    # getitem operator
    assert config["str"] == str_v
    # get with default
    assert config.get("int", int_v + 10) == int_v
    assert "int_default" not in config
    assert config.get("int_default", int_v + 15) == int_v + 15
    assert "int_default" in config
    # get with return type
    assert config.get("bool", return_type=bool) == bool_v
    assert config.get("int", return_type=int) == int_v
    assert config.get("str", return_type=str) == str_v
    # get with return type function
    assert config.get("enum", return_type=DummyEnum.__getitem__) == enum_v


def test_configuration_get_missing_key():
    config = Configuration({})
    with pytest.raises(MissingKeyError) as excinfo:
        config.get("missing")
    assert str(excinfo.value) == "Key `missing` does not exist"


def test_configuration_case_insensitivity():
    config = Configuration(
        {
            "KEY1": 1,
            "kEy2": 2,
            "key3": 3,
        }
    )
    assert config.get("key1") == 1
    assert config.get("KeY2") == 2
    assert config.get("KEY3") == 3


def test_configuration_get_num():
    int_v = 16
    float_v = 3.14
    config = Configuration({"int": int_v, "float": float_v})
    # get_num without parameters
    assert config.get_num("int") == int_v
    assert config.get_num("float") == float_v
    # get_num with type parameter
    assert config.get_int("int") == int_v
    assert config.get_float("float") == float_v
    # get_num with range parameter
    assert config.get_int("int", min_val=15, max_val=17) == int_v
    assert config.get_float("float", min_val=3.1, max_val=3.2) == float_v


def test_configuration_get_num_invalid():
    int_v = 16
    float_v = 3.14159
    str_v = "32"
    config = Configuration({"int": int_v, "float": float_v, "str": str_v})
    # get_num out of range (min)
    int_min_val_v = 2 * int_v
    int_default_v = 2 * int_min_val_v
    with pytest.raises(InvalidValueError) as excinfo:
        config.get_int("int", default_value=int_default_v, min_val=int_min_val_v)
    assert str(excinfo.value) == f"Value of key `int` is not valid: {int_v} is smaller than minimum value {int_min_val_v}"
    # get_num out of range (max)
    float_max_val_v = 0.5 * float_v
    float_default_v = 0.5 * float_max_val_v
    with pytest.raises(InvalidValueError) as excinfo:
        config.get_float("float", default_value=float_default_v, max_val=float_max_val_v)
    assert (
        str(excinfo.value) == f"Value of key `float` is not valid: {float_v} is larger than maximum value {float_max_val_v}"
    )
    # get_num not int or float
    with pytest.raises(InvalidTypeError) as excinfo:
        config.get_num("str")
    assert str(excinfo.value) == "Could not convert value of type `str` to target type for key `str`: not `int` or `float`"
    # get_int not int
    with pytest.raises(InvalidTypeError) as excinfo:
        config.get_int("float")
    assert str(excinfo.value) == "Could not convert value of type `float` to target type for key `float`: not `int`"
    # get_float not float
    with pytest.raises(InvalidTypeError) as excinfo:
        config.get_float("int")
    assert str(excinfo.value) == "Could not convert value of type `int` to target type for key `int`: not `float`"


def test_configuration_get_array():
    list_str_v = ["hello", "world"]
    empty_list_v = []
    config = Configuration({"list_str": list_str_v, "empty_list": empty_list_v})
    # get_array without parameters
    assert config.get_array("list_str") == list_str_v
    assert config.get_array("empty_list") == empty_list_v
    # get_array with type parameter
    assert config.get_array("list_str", element_type=str) == list_str_v
    assert config.get_array("empty_list", element_type=int) == empty_list_v
    # get_array with default parameter
    default_list_int_v = [1, 2, 3, 4]
    assert config.get_array("default_list_int", default_value=default_list_int_v, element_type=int) == default_list_int_v
    # invalid element
    with pytest.raises(InvalidTypeError) as excinfo:
        config.get_array("list_str", element_type=int)
    assert (
        str(excinfo.value) == "Could not convert value of type `list` to target type for key `list_str`: "
        "Could not convert value of type `str` to `int` for key `list_str[0]`"
    )


def test_configuration_get_set():
    set_int_v = {1, 2, 2, 1, 3}  # noqa: B033
    config = Configuration({"set_int": list(set_int_v)})
    # get_set without parameters
    assert config.get_set("set_int") == set_int_v
    # get_set with type parameter
    assert config.get_set("set_int", element_type=int) == set_int_v
    # get_set with default parameter
    default_set_str_v = {"A", "B", "B", "A", "C"}  # noqa: B033
    assert config.get_set("default_set_str", default_value=default_set_str_v, element_type=str) == default_set_str_v
    # invalid element
    with pytest.raises(InvalidTypeError) as excinfo:
        config.get_set("set_int", element_type=str)
    assert (
        str(excinfo.value) == "Could not convert value of type `list` to target type for key `set_int`: "
        "Could not convert value of type `int` to `str` for key `set_int['1']`"
    )


def test_configuration_get_path():
    test_files_path = pathlib.Path(__file__).parent / "test_files"
    absolute_existing_path_v = test_files_path / "good_config.toml"
    absolute_existing_path_2_v = test_files_path / "good_config.yaml"
    absolute_nonexisting_path_v = test_files_path / "nonexistent.txt"
    relative_nonexistent_path_v = pathlib.Path("nonexistent.txt")
    config = Configuration(
        {
            "absolute_existing_path": str(absolute_existing_path_v),
            "absolute_existing_path_2": str(absolute_existing_path_2_v),
            "absolute_nonexisting_path": str(absolute_nonexisting_path_v),
            "relative_nonexistent_path": str(relative_nonexistent_path_v),
            "int": 8,
        }
    )
    # get_path with check_exists
    assert config.get_path("absolute_existing_path", check_exists=True) == absolute_existing_path_v
    assert config.get_path("absolute_existing_path_2", check_exists=False) == absolute_existing_path_2_v
    assert config.get_path("absolute_nonexisting_path", check_exists=False) == absolute_nonexisting_path_v
    assert config.get_path("relative_nonexistent_path", check_exists=False) == pathlib.Path(os.getcwd()) / "nonexistent.txt"
    # get_path with default parameter
    default_path_v = test_files_path / "nonexistent_default.txt"
    assert config.get_path("default_path", default_path_v) == default_path_v
    # checked nonexisting path
    with pytest.raises(InvalidValueError) as excinfo:
        assert config.get_path("absolute_nonexisting_path", check_exists=True)
    assert (
        str(excinfo.value)
        == f"Value of key `absolute_nonexisting_path` is not valid: path `{absolute_nonexisting_path_v}` not found"
    )
    # invalid value
    with pytest.raises(InvalidValueError) as excinfo:
        config.get_path("int")
    assert str(excinfo.value) == "Value of key `int` is not valid: path needs to be a string"


def test_configuration_get_datetime():
    date_v = datetime.date(2025, 12, 7)
    time_v = datetime.time(13, 00, tzinfo=datetime.UTC)
    datetime_v = datetime.datetime.combine(date_v, time_v)
    config = Configuration(
        {
            "date": date_v,
            "time": time_v,
            "datetime": datetime_v,
        }
    )
    assert config.get("date") == datetime.datetime.combine(date_v, datetime.time(0, 0)).astimezone(datetime.UTC)
    assert config.get("time", return_type=lambda x: x.timetz()) == time_v
    assert config.get("datetime") == datetime_v


def test_configuration_get_section():
    config = Configuration(
        {
            "int": 5,
            "sub_1": {"int": 4},
            "sub_2": {
                "int": 3,
                "sub": {
                    "int": 2,
                    "sub": {  #
                        "int": 1
                    },
                },
            },
        }
    )
    # get_section
    assert config.get("int") == 5
    config_subdict_1 = config.get_section("sub_1")
    assert config_subdict_1.get("int") == 4
    config_subdict_2 = config.get_section("sub_2")
    assert config_subdict_2.get("int") == 3
    config_subsubdict = config_subdict_2.get_section("sub")
    assert config_subsubdict.get("int") == 2
    config_subsubsubdict = config_subsubdict.get_section("sub")
    assert config_subsubsubdict.get("int") == 1
    # missing key
    with pytest.raises(MissingKeyError) as excinfo:
        config_subsubsubdict.get_section("sub")
    assert str(excinfo.value) == "Key `sub_2.sub.sub.sub` does not exist"
    # invalid type
    with pytest.raises(InvalidTypeError) as excinfo:
        config_subsubsubdict.get_section("int")
    assert str(excinfo.value) == "Could not convert value of type `int` to `Section` for key `sub_2.sub.sub.int`"
    # get_section with default parameter
    config_subdict_2_def = config.get_section("sub_2", {})
    assert "int" in config_subdict_2_def
    config_subdict_3_def = config.get_section("sub_3", {"default": "True", "sub": {"int": -3}})
    assert config_subdict_3_def.get("default") == "True"
    config_subdict_3_def_sub = config_subdict_3_def.get_section("sub")
    assert config_subdict_3_def_sub.get("int") == -3
    with pytest.raises(InvalidTypeError) as excinfo:
        config.get_section("int", {})
    assert str(excinfo.value) == "Could not convert value of type `int` to `Section` for key `int`"
    # dictionary via get
    with pytest.raises(InvalidTypeError) as excinfo:
        config["sub_1"]
    assert (
        str(excinfo.value)
        == "Could not convert value of type `Section` to target type for key `sub_1`: usage of `get_section` required"
    )


def test_configuration_get_keys():
    config = Configuration({"bool": True, "int": 1, "sub_1": {"hello": 1, "world": 2}, "sub_2": {"1": 1, "2": 4, "3": 9}})
    assert set(config.get_keys()) == {"bool", "int", "sub_1", "sub_2"}


def test_configuration_aliases():
    # Alias used
    config_old = Configuration({"old": 1})
    assert "old" in config_old
    config_old.set_alias("new", "old")
    assert "old" not in config_old
    assert config_old.get("new") == 1
    # Alias not used
    config_new = Configuration({"new": 1})
    assert "new" in config_new
    config_new.set_alias("new", "old")
    assert "old" not in config_new
    assert config_new.get("new") == 1
    # Alias not in configuration
    config = Configuration({"something": "else"})
    config.set_alias("new", "old")
    assert "old" not in config
    assert "new" not in config


def test_configuration_unused_keys():
    config = Configuration(
        {
            "used": 1024,
            "unused": 1024,
            "sub": {  #
                "used": 2048,
                "unused": 2048,
                "used_empty": {},
                "sub": {  #
                    "unused": 4096
                },
            },
        }
    )
    # Mark keys as used
    assert config.get("used") == 1024
    config_sub = config.get_section("sub")
    assert config_sub.get("used") == 2048
    assert len(config_sub.get_section("used_empty")) == 0
    # Check existence of unused keys
    assert "unused" in config
    assert "unused" in config_sub
    # Remove unused keys
    removed_keys = config._remove_unused_entries()
    assert set(removed_keys) == {"unused", "sub.unused", "sub.sub"}
    # Check unused keys were removed
    assert "unused" not in config
    config_sub_after = config.get_section("sub")
    assert "used" in config_sub_after
    assert "used_empty" in config_sub_after
    assert "unused" not in config_sub_after
    assert "sub" not in config_sub_after


def test_configuration_update():
    config = Configuration(
        {
            "bool": True,
            "int": 1024,
            "array_empty": [],
            "array_int": [1, 2, 3],
            "array_int2": [1, 2, 3, 4, 5],
            "sub": {  #
                "float": 3.14,
                "string": "test",
            },
        }
    )
    config_update = Configuration(
        {
            "bool": False,
            "int": 2048,
            "array_empty": [1, 2],
            "array_int": [],
            "array_int2": [1, 2, 3, 4],
            "sub": {  #
                "float": 6.28,
            },
        }
    )
    config._update(config_update)
    assert config.get("bool") == False  # noqa: E712
    assert config.get("int") == 2048
    assert config.get("array_empty") == [1, 2]
    assert config.get("array_int") == []
    assert config.get("array_int2") == [1, 2, 3, 4]
    config_sub = config.get_section("sub")
    assert config_sub.get("float") == 6.28
    assert config_sub.get("string") == "test"


def test_configuration_update_failure():
    config = Configuration({"sub": {"bool": True, "int": 1024, "array": [1.5, 2.5, 3.5]}})
    # Updating non-existing key
    config_ne_key = Configuration({"sub": {"bool2": False}})
    with pytest.raises(InvalidUpdateError) as excinfo:
        config._update(config_ne_key)
    assert str(excinfo.value) == "Failed to update value of key `sub.bool2`: key does not exist in current configuration"
    # Updating with other type
    config_other_type = Configuration({"sub": {"bool": 5}})
    with pytest.raises(InvalidUpdateError) as excinfo:
        config._update(config_other_type)
    assert str(excinfo.value) == "Failed to update value of key `sub.bool`: cannot change type from `bool` to `int`"
    # Update with other element type in array
    config_other_element_type = Configuration({"sub": {"array": ["hello", "world"]}})
    with pytest.raises(InvalidUpdateError) as excinfo:
        config._update(config_other_element_type)
    assert (
        str(excinfo.value)
        == "Failed to update value of key `sub.array`: cannot change type from `list[float]` to `list[str]`"
    )


def test_configuration_str():
    config_dict = {"hello": "world"}
    config = Configuration(config_dict)
    assert str(config_dict) == str(config)


def test_configuration_to_string():
    config = Configuration(
        {
            "_internal": 1024,
            "_internal_section": {  #
                "key": "internal"
            },
            "user": 3.14,
            "sub_1": {  #
                "array": [1, 2, 3, 4]
            },
            "sub_2": {
                "sub": {  #
                    "string": "hello world"
                }
            },
        }
    )
    str_all = config.to_string(ConfigurationGroup.ALL)
    assert "\n  _internal: 1024" in str_all
    assert "\n  _internal_section:\n    key: internal" in str_all
    assert "\n  user: 3.14" in str_all
    assert "\n  sub_1:\n    array: [1, 2, 3, 4]" in str_all
    assert "\n  sub_2:\n    sub:\n      string: hello world" in str_all
    str_user = config.to_string(ConfigurationGroup.USER)
    assert "_internal" not in str_user
    assert "\n  user: 3.14" in str_user
    assert "\n  sub_1:\n    array: [1, 2, 3, 4]" in str_user
    assert "\n  sub_2:\n    sub:\n      string: hello world" in str_user
    str_internal = config.to_string(ConfigurationGroup.INTERNAL)
    assert "\n  _internal: 1024" in str_internal
    assert "\n  _internal_section:\n    key: internal" in str_internal
    assert "user" not in str_internal


def test_configuration_assemble_disassemble():
    config = Configuration({"bool": True, "int": 5, "sub": {"hello": 1, "world": 2}, "datetime": datetime.time(12, 00)})
    config_disassembled = Configuration.disassemble(config.assemble())
    assert config._dictionary == config_disassembled._dictionary


def test_configuration_env_variables():
    # set environment variable
    os.environ["CNSTLN_TEST_KEY"] = "value"

    config = Configuration(
        {
            "env_var": "${CNSTLN_TEST_KEY}",
            "env_var_default": "${CNSTLN_TEST_KEY:-default}",
            "env_var_prefix": "prefix_$${CNSTLN_TEST_KEY}",
            "env_var_multi": "${CNSTLN_TEST_KEY}&${CNSTLN_TEST_KEY}",
            "env_var_escaped": "\\${CNSTLN_TEST_KEY}",
            "env_var_missing": "${MISSING}",
            "env_var_missing_default": "${MISSING:-default}",
        }
    )

    assert config["env_var"] == "value"
    assert config["env_var_default"] == "value"
    assert config["env_var_prefix"] == "prefix_$value"
    assert config["env_var_multi"] == "value&value"
    assert config["env_var_escaped"] == "${CNSTLN_TEST_KEY}"
    with pytest.raises(ValueError) as e:
        config["env_var_missing"]
    assert "Environment variable `MISSING` not defined" in str(e.value)
    assert config["env_var_missing_default"] == "default"


def test_validate_dictionary_invalid():
    # Not a dictionary
    with pytest.raises(TypeError) as excinfo:
        Configuration("str")  # type: ignore
    assert str(excinfo.value) == "not a dictionary"
    # Keys not string
    with pytest.raises(TypeError) as excinfo:
        Configuration({"sub": {1: 1, 2: 4, 3: 9}})  # type: ignore
    assert str(excinfo.value) == "key `sub.1` not a string"
    # Duplicate keys with different casing
    with pytest.raises(ValueError) as excinfo:
        Configuration({"sub": {"one": 1, "ONE": 1}})
    assert str(excinfo.value) == "key `sub.one` already present"
    # Inhomogeneous array
    with pytest.raises(TypeError) as excinfo:
        Configuration({"sub": {"array": [1, "two", 3.0]}})
    assert str(excinfo.value) == "array value of key `sub.array` not homogeneous"
    # Nested arrays
    with pytest.raises(TypeError) as excinfo:
        Configuration({"sub": {"array": [[1, 2], [3, 4]]}})
    assert str(excinfo.value) == "elements of array value of key `sub.array` not scalar"
    # Non-scalar types
    with pytest.raises(TypeError) as excinfo:
        Configuration({"sub": {"class": object()}})
    assert str(excinfo.value) == "value of key `sub.class` not scalar"
