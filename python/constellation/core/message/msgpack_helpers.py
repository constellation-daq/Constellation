"""
SPDX-FileCopyrightText: 2025 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2

MessagePack decoding helpers
"""

from datetime import datetime
from enum import IntEnum
from typing import Any, Type, TypeVar

import msgpack  # type: ignore[import-untyped]

from .exceptions import MessageDecodingError

T = TypeVar("T")
T_IE = TypeVar("T_IE", bound=IntEnum)


def msgpack_unpack_to(unpacker: msgpack.Unpacker, target_type: Type[T]) -> T:
    value = None
    try:
        value = unpacker.unpack()
    except msgpack.UnpackException as e:
        raise MessageDecodingError("Error unpacking data") from e
    if type(value) is not target_type:
        raise MessageDecodingError(f"Type error: expected {target_type} but got {type(value)}")
    return value


def msgpack_unpack_to_int_enum(unpacker: msgpack.Unpacker, target_type: Type[T_IE]) -> T_IE:
    assert issubclass(target_type, IntEnum)
    value = msgpack_unpack_to(unpacker, int)
    try:
        value = target_type(value)
    except ValueError as e:
        raise MessageDecodingError(f"Type error: `{value}` is not a valid value for {target_type}") from e
    return value


def convert_from_msgpack_timestamp(tags: dict[str, Any]) -> dict[str, Any]:
    def to_datetime(value: Any) -> Any:
        if isinstance(value, msgpack.Timestamp):
            return value.to_datetime()
        return value

    return {key: to_datetime(value) for key, value in tags.items()}


def convert_to_msgpack_timestamp(tags: dict[str, Any]) -> dict[str, Any]:
    def from_datetime(value: Any) -> Any:
        if isinstance(value, datetime):
            return msgpack.Timestamp.from_datetime(value)
        return value

    return {key: from_datetime(value) for key, value in tags.items()}
