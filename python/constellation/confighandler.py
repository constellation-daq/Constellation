"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0
"""

from collections.abc import MutableMapping

import tomllib


class ConfigError(Exception):
    pass


class Configuration:
    """Class to track configuration variables and requests."""

    def __init__(self, config: dict = {}):
        """Initialize configuration variables"""
        if not isinstance(config, dict):
            raise ConfigError
        self._config = config
        self._requested_keys: set[str] = set()

    def has_unused_values(self):
        """Check if any unused configuration keys exist."""
        return not self._requested_keys == set(self._config.keys())

    def get_unused_keys(self):
        """Return all unused configuration keys"""
        return set(self._config.keys()).difference(self._requested_keys)

    def setdefault(self, key: str, default: any = None):
        """
        Return value from requested key in config with default value if specified.
        Mark key as requested in configuration.
        """
        self._requested_keys.add(key)
        return self._config.setdefault(key, default)

    def __getitem__(self, key: str):
        """
        Return value from requested key in config.
        Mark key as requested in configuration.
        """
        self._requested_keys.add(key)
        return self._config[key]

    def get_keys(self):
        """Return list of keys in config."""
        return list(self._config.keys())


def pack_config(dictionary, parent_key="", separator="."):
    """Pack config dict into a flat dict"""
    items = []
    for key, value in dictionary.items():
        new_key = parent_key + separator + key if parent_key else key
        if isinstance(value, MutableMapping):
            items.extend(pack_config(value, new_key, separator=separator).items())
        else:
            items.append((new_key, value))
    return dict(items)


def read_config(config_path: str):
    """Get config contents as a flat dict"""
    try:
        with open(config_path, "rb") as f:
            return tomllib.load(f)
    # TODO: Handle errors FileNotFoundError, TypeError, tomllibDecodeError
    except tomllib.TOMLDecodeError:
        raise


def filter_config(trait: str, config: dict):
    """Filter through a flat config after specific trait"""
    res = {}
    for key, value in config.items():
        if trait in key:
            res[key] = value

    return res
