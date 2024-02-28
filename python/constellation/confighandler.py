"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0
"""

from collections.abc import MutableMapping

import tomllib
import logging

logger = logging.getLogger(__name__)


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
