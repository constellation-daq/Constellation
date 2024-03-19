"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0
"""

from collections.abc import MutableMapping

import tomllib


class ConfigError(Exception):
    pass


class Configuration:
    def __init__(self, config: dict = {}):
        if not isinstance(config, dict):
            raise ConfigError
        self.config = config
        self._requested_keys: set[str] = set()

    def has_unused_values(self):
        return not self._requested_keys == set(self.config.keys())

    def get_unused_values(self):
        return set(self.config.keys()).difference(self._requested_keys)

    def get(self, key: str, default: any = None):
        self._requested_keys.add(key)
        return self.config.get(key, default)

    def __getitem__(self, key: str):
        self._requested_keys.add(key)
        return self.config[key]

    def get_values(self):
        return self.config


class ConfigReceiver:
    def __init__(self):
        self.config = {}

    def update_config(self, new_config: dict):
        """Set corresponding key/value pairs of new_config in config"""
        for key, value in new_config.items():
            self.config[key] = value

    def set_config(self):
        """Set parameter values based on self.config"""
        raise NotImplementedError


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
