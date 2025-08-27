"""
SPDX-FileCopyrightText: 2025 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2

MessagePack decoding helpers
"""

from enum import IntEnum
from typing import TypeVar

import msgpack  # type: ignore[import-untyped]

from .exceptions import MessageDecodingError

T = TypeVar("T")
T_IE = TypeVar("T_IE", bound=IntEnum)


def msgpack_unpack_to(unpacker: msgpack.Unpacker, target_type: type[T]) -> T:
    value = None
    try:
        value = unpacker.unpack()
    except msgpack.UnpackException as e:
        raise MessageDecodingError("Error unpacking data") from e
    if type(value) is not target_type:
        raise MessageDecodingError(f"Type error: expected {target_type} but got {type(value)}")
    return value


def msgpack_unpack_to_int_enum(unpacker: msgpack.Unpacker, target_type: type[T_IE]) -> T_IE:
    assert issubclass(target_type, IntEnum)
    value = msgpack_unpack_to(unpacker, int)
    try:
        value = target_type(value)
    except ValueError as e:
        raise MessageDecodingError(f"Type error: `{value}` is not a valid value for {target_type}") from e
    return value
