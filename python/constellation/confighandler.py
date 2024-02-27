"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0
"""

from collections.abc import MutableMapping

import toml


def flatten_dict(dictionary, parent_key="", separator="."):
    """Help-function for flattening dictionaries"""
    items = []
    for key, value in dictionary.items():
        new_key = parent_key + separator + key if parent_key else key
        if isinstance(value, MutableMapping):
            items.extend(flatten_dict(value, new_key, separator=separator).items())
        else:
            items.append((new_key, value))
    return dict(items)


def read_config(config_path: str):
    """Get config contents as a flat dict"""
    try:
        config_dict = toml.load(config_path)
        return flatten_dict(config_dict)
    #   TODO: Handle errors FileNotFoundError, TypeError, TomlDecodeError
    except toml.TomlDecodeError:
        raise
