"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2

Module implementing the Constellation communication protocols.
"""

import io
import time
from enum import StrEnum
from typing import Any

import msgpack  # type: ignore[import-untyped]
import zmq


class Protocol(StrEnum):
    CDTP1 = "CDTP\x01"
    CDTP2 = "CDTP\x02"
    CSCP1 = "CSCP\x01"
    CMDP1 = "CMDP\x01"
    CHP1 = "CHP\x01"


class MessageHeader:
    """Class implementing a Constellation message header."""

    def __init__(self, name: str, protocol: Protocol):
        self.name: str = name
        self.protocol: Protocol = protocol

    def send(
        self,
        socket: zmq.Socket,  # type: ignore[type-arg]
        flags: int = zmq.SNDMORE,
        meta: dict[str, Any] | None = None,
        **kwargs: Any,
    ) -> None:
        """Send a message header via socket.

        meta is an optional dictionary that is sent as a map of string/value
        pairs with the header.

        """
        socket.send(self.encode(meta, **kwargs), flags)

    def recv(
        self, socket: zmq.Socket, flags: int = 0  # type: ignore[type-arg]
    ) -> tuple[str, msgpack.Timestamp, dict[str, Any] | None] | tuple[str, int, int, dict[str, Any] | None]:
        """Receive header from socket and return all decoded fields."""
        return self.decode(socket.recv())

    def decode(
        self, header: Any
    ) -> tuple[str, msgpack.Timestamp, dict[str, Any] | None] | tuple[str, int, int, dict[str, Any] | None]:
        """Decode header string and return host, timestamp and meta map."""
        unpacker = msgpack.Unpacker()
        unpacker.feed(header)
        protocol = unpacker.unpack()
        if not protocol == self.protocol.value:
            raise RuntimeError(f"Received message with malformed {self.protocol.name} header: {bytes(header)!r}!")
        host = unpacker.unpack()
        if protocol == Protocol.CDTP1:
            msgtype = unpacker.unpack()
            seqno = unpacker.unpack()
            meta = unpacker.unpack()
            return host, msgtype, seqno, meta
        timestamp = unpacker.unpack()
        meta = unpacker.unpack()
        return host, timestamp, meta

    def encode(self, meta: dict[str, Any] | None = None, **kwargs: Any) -> memoryview:
        """Generate and return a header as list.

        Additional keyword arguments are required for protocols specifying
        additional fields.

        """
        if not meta:
            meta = {}
        stream = io.BytesIO()
        packer = msgpack.Packer()
        stream.write(packer.pack(self.protocol.value))
        stream.write(packer.pack(self.name))
        if self.protocol != Protocol.CDTP1:
            stream.write(packer.pack(msgpack.Timestamp.from_unix_nano(time.time_ns())))
        else:
            stream.write(packer.pack(kwargs["msgtype"]))
            stream.write(packer.pack(kwargs["seqno"]))
        stream.write(packer.pack(meta))
        return stream.getbuffer()
