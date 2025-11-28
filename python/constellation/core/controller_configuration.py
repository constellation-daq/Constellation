"""
SPDX-FileCopyrightText: 2025 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2

Controller configuration class
"""

from __future__ import annotations

import copy
import enum
import pathlib
import tomllib
from typing import Any

import yaml

from constellation.core.configuration import Configuration


class ConfigParseError(RuntimeError):
    def __init__(self, reason: str):
        super().__init__(f"Could not parse configuration: {reason}")


class ConfigKeyError(ConfigParseError):
    def __init__(self, key: str, reason: str):
        super().__init__(f"error while parsing key `{key}`: {reason}")


class ConfigValueError(ConfigParseError):
    def __init__(self, key: str, reason: str):
        super().__init__(f"error while parsing value of key `{key}`: {reason}")


class ConfigValidationError(ConfigParseError):
    def __init__(self, reason: str):
        super().__init__(f"error validating configuration: {reason}")


def _convert_to_config(config: Configuration | dict[str, Any]) -> Configuration:
    if not isinstance(config, Configuration):
        try:
            return Configuration(config)
        except Exception as e:
            raise ConfigParseError(str(e)) from e
    return config


def _override_dict(base_dict: dict[str, Any], new_dict: dict[str, Any], prefix: str = "") -> None:
    for key, value in new_dict.items():
        if key in base_dict:
            # Overwrite existing parameter
            prefixed_key = prefix + key
            base_value = base_dict[key]
            value_is_dict = isinstance(value, dict)
            base_value_is_dict = isinstance(base_value, dict)
            if value_is_dict or base_value_is_dict:
                if value_is_dict != base_value_is_dict:
                    raise ConfigValidationError(f"value of key `{prefixed_key}` has mismatched types when merging defaults")
                # If dictionary, overwrite recursively
                _override_dict(base_value, value, prefixed_key + ".")
            else:
                # Otherwise overwrite directly (ignore type mismatch)
                base_dict[key] = value
        else:
            # Add new parameter
            base_dict[key] = value


class FileType(enum.Enum):
    UNKNOWN = enum.auto()
    TOML = enum.auto()
    YAML = enum.auto()


class ControllerConfiguration:
    def __init__(self) -> None:
        self._global_config = Configuration()
        self._type_configs = dict[str, Configuration]()
        self._satellite_configs = dict[str, Configuration]()

    def __str__(self) -> str:
        return str(self._as_single_dict())

    @staticmethod
    def from_string(config_string: str, file_type: FileType) -> ControllerConfiguration:
        """Create a controller configuration from a string in TOML or YAML"""
        config = ControllerConfiguration()
        if file_type == FileType.YAML:
            config._parse_yaml(config_string)
        else:
            # Assume UNKNOWN is TOML
            config._parse_toml(config_string)
        return config

    @staticmethod
    def from_path(path: pathlib.Path) -> ControllerConfiguration:
        """Create a controller configuration from a file"""
        file_type = FileType.UNKNOWN
        suffix = path.suffix
        if suffix == ".toml":
            file_type = FileType.TOML
        elif suffix in [".yaml", ".yml"]:
            file_type = FileType.YAML
        with open(path) as file:
            return ControllerConfiguration.from_string(file.read(), file_type)

    def set_global_configuration(self, config: Configuration | dict[str, Any]) -> None:
        """Set the global configuration section"""
        self._global_config = _convert_to_config(config)

    def get_global_configuration(self) -> Configuration:
        """Get the global configuration section"""
        return self._global_config

    def has_type_configuration(self, satellite_type: str) -> bool:
        """Check if an explicit configuration exists for a given satellite type"""
        return satellite_type.lower() in self._type_configs

    def add_type_configuration(self, satellite_type: str, config: Configuration | dict[str, Any]) -> None:
        """Add or override an explicit configuration for a satellite type"""
        self._type_configs[satellite_type.lower()] = _convert_to_config(config)

    def get_type_configuration(self, satellite_type: str) -> Configuration:
        """Get configuration for a given satellite type"""
        satellite_type_lc = satellite_type.lower()

        # Copy global config
        config = Configuration(copy.deepcopy(self._global_config._dictionary))

        # Add parameters from type level
        if satellite_type_lc in self._type_configs:
            _override_dict(config._dictionary, self._type_configs[satellite_type_lc]._dictionary)

        return config

    def has_satellite_configuration(self, canonical_name: str) -> bool:
        """Check if an explicit configuration exists for a given satellite"""
        return canonical_name.lower() in self._satellite_configs

    def add_satellite_configuration(self, canonical_name: str, config: Configuration | dict[str, Any]) -> None:
        """Add or override an explicit configuration for a satellite"""
        self._satellite_configs[canonical_name.lower()] = _convert_to_config(config)

    def get_satellite_configuration(self, canonical_name: str) -> Configuration:
        """Get configuration for a given satellite"""
        canonical_name_lc = canonical_name.lower()

        # Find type from canonical name
        satellite_type_lc = canonical_name_lc.split(".")[0]

        # Get configuration from global and type level
        config = self.get_type_configuration(satellite_type_lc)

        # Add parameters from satellite level
        if canonical_name_lc in self._satellite_configs:
            _override_dict(config._dictionary, self._satellite_configs[canonical_name_lc]._dictionary)

        return config

    def _parse_dict(self, config_dict: dict[Any, Any]) -> None:
        has_global_default_config = False
        for type_key, type_value in config_dict.items():
            if not isinstance(type_key, str):
                raise ConfigKeyError(str(type_key), "key not a string")
            type_key_lc = type_key.lower()

            if not isinstance(type_value, dict):
                raise ConfigValueError(type_key_lc, "expected a dictionary at type level")

            if type_key_lc == "_default":
                # Global default config
                if has_global_default_config:
                    raise ConfigKeyError(type_key_lc, "key defined twice")
                self.set_global_configuration(type_value)
                has_global_default_config = True
            else:
                # Type level
                for name_key, name_value in type_value.items():
                    if not isinstance(name_key, str):
                        raise ConfigKeyError(f"{type_key_lc}.{name_key}", "key not a string")
                    name_key_lc = name_key.lower()
                    canonical_name_lc = f"{type_key_lc}.{name_key_lc}"

                    if not isinstance(name_value, dict):
                        raise ConfigValueError(canonical_name_lc, "expected a dictionary at satellite level")

                    if name_key_lc == "_default":
                        # Type default config
                        if self.has_type_configuration(type_key_lc):
                            raise ConfigKeyError(canonical_name_lc, "key defined twice")
                        self.add_type_configuration(type_key_lc, name_value)
                    else:
                        # Satellite level
                        if self.has_satellite_configuration(canonical_name_lc):
                            raise ConfigKeyError(canonical_name_lc, "key defined twice")
                        self.add_satellite_configuration(canonical_name_lc, name_value)

    def _parse_toml(self, config_string: str) -> None:
        toml_dict: dict[str, Any]
        try:
            toml_dict = tomllib.loads(config_string)
        except Exception as e:
            raise ConfigParseError(str(e)) from e
        self._parse_dict(toml_dict)

    def _parse_yaml(self, config_string: str) -> None:
        # Workaround for YAML
        def adjust_parsed_yaml_dict(yaml_dict: dict[Any, Any]) -> None:
            for key, value in yaml_dict.items():
                if value is None:
                    # Interpret empty node as empty dictionary
                    yaml_dict[key] = {}
                elif isinstance(value, dict):
                    # Run recursively
                    adjust_parsed_yaml_dict(value)

        yaml_dict: Any
        try:
            yaml_dict = yaml.safe_load(config_string)
        except Exception as e:
            raise ConfigParseError(str(e)) from e
        if yaml_dict is None:
            yaml_dict = {}
        if not isinstance(yaml_dict, dict):
            raise ConfigParseError("expected map as root node")
        adjust_parsed_yaml_dict(yaml_dict)
        self._parse_dict(yaml_dict)

    def _as_single_dict(self) -> dict[str, Any]:
        single_dict = {"_default": self._global_config._dictionary}
        for key, value in self._type_configs.items():
            single_dict[key] = {"_default": value._dictionary}
        for key, value in self._satellite_configs.items():
            satellite_type, satellite_name = key.split(".")
            if satellite_type not in single_dict:
                single_dict[satellite_type] = {}
            single_dict[satellite_type][satellite_name] = value._dictionary
        return single_dict


def load_config(path: str | pathlib.Path) -> ControllerConfiguration:
    """Load a configuration file"""
    if isinstance(path, str):
        path = pathlib.Path(path)
    return ControllerConfiguration.from_path(path)
