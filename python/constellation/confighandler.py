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

        if key in self.config.keys():
            return self.config[key]
        elif default:
            self.config[key] = default
            return self.config[key]
        else:
            raise KeyError

    def __getitem__(self, key: str):
        self._requested_keys.add(key)
        if key in self.config.keys():
            return self.config[key]
        else:
            raise KeyError


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


def unpack_config(dictionary, separator="."):
    """Unpack config dict from flat dict"""
    result = dict()
    for key, value in dictionary.items():
        # Split key into each separate part
        parts = key.split(separator)

        # Parse key parts and either add key to items
        # or create new dict for the part
        d = result
        for part in parts[:-1]:
            if part not in d:
                d[part] = dict()
            d = d[part]
        d[parts[-1]] = value
    return result


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
