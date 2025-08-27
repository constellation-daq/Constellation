"""
SPDX-FileCopyrightText: 2025 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2

Provides message class for CSCP1
"""

from __future__ import annotations

from datetime import datetime
from enum import Enum, IntEnum
from io import BytesIO
from typing import Any

import msgpack  # type: ignore[import-untyped]

from ..protocol import Protocol
from .exceptions import InvalidProtocolError, MessageDecodingError, UnexpectedProtocolError
from .msgpack_helpers import msgpack_unpack_to, msgpack_unpack_to_int_enum
from .multipart import MultipartMessage


class SatelliteState(Enum):
    """Available states to cycle through."""

    # Idle state without any configuration
    NEW = 0x10
    # Initialized state with configuration but not (fully) applied
    INIT = 0x20
    # Prepared state where configuration is applied
    ORBIT = 0x30
    # Running state where DAQ is running
    RUN = 0x40
    # Safe fallback state if error is discovered during run
    SAFE = 0xE0
    # Error state if something went wrong
    ERROR = 0xF0
    #
    #  TRANSITIONAL STATES
    #
    initializing = 0x12
    launching = 0x23
    landing = 0x32
    reconfiguring = 0x33
    starting = 0x34
    stopping = 0x43
    interrupting = 0x0E
    # state if shutdown
    DEAD = 0xFF

    def transitions_to(self, state: Enum) -> bool:
        # Target steady state indicated by the lower four bits
        return bool(((self.value & 0x0F) << 4) == state.value)


class CSCP1Message:
    """Message class for CSCP1"""

    class Type(IntEnum):
        """Enum describing the type of CSCP1 message"""

        REQUEST = 0x0
        """Request with a command"""

        SUCCESS = 0x1
        """Command is being executed"""

        NOTIMPLEMENTED = 0x2
        """Command is valid but not implemented"""

        INCOMPLETE = 0x3
        """Command is valid but mandatory payload information is missing or incorrectly formatted"""

        INVALID = 0x4
        """Command is invalid for the current state"""

        UNKNOWN = 0x5
        """Command is entirely unknown"""

        ERROR = 0x6
        """Previously received message is invalid"""

    def __init__(
        self,
        sender: str,
        verb: tuple[CSCP1Message.Type, str],
        time: datetime | None = None,
        tags: dict[str, Any] | None = None,
    ):
        self._protocol = Protocol.CSCP1
        self._sender = sender
        self._time = time if time is not None else datetime.now().astimezone()
        self._tags = tags if tags is not None else {}
        self._verb = verb
        self._payload = None

    @property
    def sender(self) -> str:
        return self._sender

    @property
    def time(self) -> datetime:
        return self._time

    @property
    def tags(self) -> dict[str, Any]:
        return self._tags

    @property
    def verb(self) -> tuple[CSCP1Message.Type, str]:
        return self._verb

    @property
    def verb_type(self) -> CSCP1Message.Type:
        return self._verb[0]

    @property
    def verb_msg(self) -> str:
        return self._verb[1]

    @property
    def payload(self) -> Any:
        return self._payload

    @payload.setter
    def payload(self, payload: Any) -> None:
        self._payload = payload

    def assemble(self) -> MultipartMessage:
        streams = []
        packer = msgpack.Packer(datetime=True)

        # Pack header
        header_stream = BytesIO()
        header_stream.write(packer.pack(self._protocol.value))
        header_stream.write(packer.pack(self._sender))
        header_stream.write(packer.pack(self._time))
        header_stream.write(packer.pack(self._tags))
        streams.append(header_stream)
        # Pack verb
        verb_stream = BytesIO()
        verb_stream.write(packer.pack(self._verb[0].value))
        verb_stream.write(packer.pack(self._verb[1]))
        streams.append(verb_stream)
        # Pack payload
        if self._payload is not None:
            payload_stream = BytesIO()
            payload_stream.write(packer.pack(self._payload))
            streams.append(payload_stream)

        return MultipartMessage(streams)

    @staticmethod
    def disassemble(frames: list[bytes]) -> CSCP1Message:
        unpacker = msgpack.Unpacker(timestamp=3)
        if len(frames) not in [2, 3]:
            raise MessageDecodingError(f"Expected 2 or 3 frames, got {len(frames)}")

        # Unpack header
        unpacker.feed(frames[0])
        protocol = msgpack_unpack_to(unpacker, str)
        try:
            protocol = Protocol(protocol)
        except ValueError as e:
            raise InvalidProtocolError(protocol) from e
        if protocol is not Protocol.CSCP1:
            raise UnexpectedProtocolError(protocol, Protocol.CSCP1)
        sender = msgpack_unpack_to(unpacker, str)
        time = msgpack_unpack_to(unpacker, datetime)
        tags = msgpack_unpack_to(unpacker, dict)
        # Unpack verb
        unpacker.feed(frames[1])
        verb_type = msgpack_unpack_to_int_enum(unpacker, CSCP1Message.Type)
        verb_msg = msgpack_unpack_to(unpacker, str)
        # Unpack payload
        payload = None
        if len(frames) == 3:
            unpacker.feed(frames[2])
            payload = unpacker.unpack()

        # Assemble and return message
        msg = CSCP1Message(sender, (verb_type, verb_msg), time, tags)
        if payload is not None:
            msg.payload = payload
        return msg

    def __str__(self) -> str:
        payload_str = f" and payload `{self._payload}`" if self._payload is not None else ""
        return (
            f"CSCP1 Message with verb `{self._verb[0].name}`:`{self._verb[1]}`{payload_str}"
            f" from {self._sender} received at {self._time}"
        )
