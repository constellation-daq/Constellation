"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2
"""

from __future__ import annotations

import io
import time
from enum import Enum, IntFlag, auto
from threading import Lock

import msgpack  # type: ignore[import-untyped]
import zmq

from .protocol import Protocol


class CHPMessageFlags(IntFlag):
    """Defines the message flags of CHP messages."""

    NONE = 0x00
    DENY_DEPARTURE = 0x01
    TRIGGER_INTERRUPT = 0x02
    MARK_DEGRADED = 0x04
    IS_EXTRASYSTOLE = 0x80


class CHPRole(Enum):
    """Defines the role of the satellite."""

    NONE = auto()
    TRANSIENT = auto()
    DYNAMIC = auto()
    ESSENTIAL = auto()

    @classmethod
    def from_flags(cls, flags: CHPMessageFlags) -> CHPRole:
        if flags & CHPMessageFlags.MARK_DEGRADED:
            if flags & CHPMessageFlags.TRIGGER_INTERRUPT:
                if flags & CHPMessageFlags.DENY_DEPARTURE:
                    return CHPRole.ESSENTIAL
                return CHPRole.DYNAMIC
            return CHPRole.TRANSIENT
        return CHPRole.NONE

    def flags(self) -> CHPMessageFlags:
        if self == CHPRole.TRANSIENT:
            return CHPMessageFlags.MARK_DEGRADED
        if self == CHPRole.DYNAMIC:
            return CHPMessageFlags.MARK_DEGRADED | CHPMessageFlags.TRIGGER_INTERRUPT
        if self == CHPRole.ESSENTIAL:
            return CHPMessageFlags.MARK_DEGRADED | CHPMessageFlags.TRIGGER_INTERRUPT | CHPMessageFlags.DENY_DEPARTURE
        return CHPMessageFlags.NONE

    def role_requires(self, flags: CHPMessageFlags) -> bool:
        return bool(flags & self.flags())


def CHPDecodeMessage(msg: list[bytes]) -> tuple[str, msgpack.Timestamp, int, CHPMessageFlags, int, str | None]:
    """Decode a CHP binary message.

    Returns host, timestamp, state, interval and status if available.

    """
    unpacker = msgpack.Unpacker()
    unpacker.feed(msg[0])
    protocol = unpacker.unpack()
    if not protocol == Protocol.CHP1.value:
        raise RuntimeError(f"Received message with malformed CHP header: {protocol}!")

    name = unpacker.unpack()
    timestamp = unpacker.unpack()
    state = unpacker.unpack()
    flags = CHPMessageFlags(unpacker.unpack())
    interval = unpacker.unpack()

    status = None
    if len(msg) > 1:
        status = msg[1].decode("utf-8")

    return name, timestamp, state, flags, interval, status


class CHPTransmitter:
    """Send and receive via the Constellation Heartbeat Protocol (CHP)."""

    def __init__(self, name: str, socket: zmq.Socket):  # type: ignore[type-arg]
        """Initialize transmitter."""
        self.name = name
        self._socket = socket
        self._socket_lock = Lock()

    def send(self, state: int, interval: int, msgflags: CHPMessageFlags, status: str | None = None, flags: int = 0) -> None:
        """Send state and interval via CHP."""
        stream = io.BytesIO()
        packer = msgpack.Packer()
        stream.write(packer.pack(Protocol.CHP1))
        stream.write(packer.pack(self.name))
        stream.write(packer.pack(msgpack.Timestamp.from_unix_nano(time.time_ns())))
        stream.write(packer.pack(state))
        stream.write(packer.pack(msgflags))
        stream.write(packer.pack(interval))

        if status:
            flags = zmq.SNDMORE | flags

        with self._socket_lock:
            self._socket.send(stream.getbuffer(), flags=flags)
            if status:
                flags = flags & ~zmq.SNDMORE
                self._socket.send_string(status, flags)

    def parse_subscriptions(self) -> int:
        subscriptions: int = 0
        while True:
            try:
                with self._socket_lock:
                    msg = self._socket.recv(zmq.NOBLOCK)
                subscriptions += 1 if msg == b"\x01" else -1
            except zmq.ZMQError:
                break
        return subscriptions

    def recv(
        self, flags: int = zmq.NOBLOCK
    ) -> tuple[str, msgpack.Timestamp, int, CHPMessageFlags, int, str | None] | tuple[None, None, None, None, None]:
        """Receive a heartbeat via CHP."""
        try:
            with self._socket_lock:
                msg = self._socket.recv_multipart(flags)
        except zmq.ZMQError as e:
            if "Resource temporarily unavailable" not in e.strerror:
                raise RuntimeError("CommandTransmitter encountered zmq exception") from e
            return None, None, None, None, None
        return CHPDecodeMessage(msg)

    def close(self) -> None:
        """Close the socket of the transmitter."""
        with self._socket_lock:
            self._socket.close()
