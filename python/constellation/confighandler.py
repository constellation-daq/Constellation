"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0
"""

from collections.abc import MutableMapping

import tomllib
import logging

logger = logging.getLogger(__name__)


class Config:
    def __init__(self, config: dict):
        self.config = config

    def set_config(self, config: dict, separator="."):
        """Set corresponding key/value pairs of args in config"""

        for key, value in config.items():
            if config[key]:
                config[key] = value


def unpack_config(dictionary, base, separator="."):
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
