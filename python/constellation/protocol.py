#!/usr/bin/env python3
"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0

Module implementing the Constellation communication protocols.
"""

import time
from enum import StrEnum

import msgpack
import zmq


class Protocol(StrEnum):
    CDTP = "CDTP%x01"
    CSCP = "CDTP%x01"
    CMDP = "CMDP\01"


class MessageHeader:
    """Class implementing a Constellation message header."""

    def __init__(self, name: str, protocol: Protocol):
        self.name = name
        self.protocol = protocol

    def send(self, socket: zmq.Socket, flags: int = zmq.SNDMORE, meta: dict = None):
        """Send a message header via socket.

        meta is an optional dictionary that is sent as a map of string/value
        pairs with the header.

        Returns: return value from socket.send().

        """
        return socket.send(self.encode(meta), flags)

    def recv(self, socket: zmq.Socket, flags: int = 0):
        """Receive header from socket and return all decoded fields."""
        return self.decode(socket.recv())

    def decode(self, header):
        """Decode header string and return host, timestamp and meta map."""
        header = msgpack.unpackb(header)
        if not header[0] == self.protocol.value:
            raise RuntimeError(
                f"Received message with malformed {self.protocol.name} header: {header}!"
            )
        host = header[1]
        timestamp = header[2]
        meta = header[3]
        return host, timestamp, meta

    def encode(self, meta: dict = None):
        """Generate and return a header as list."""
        if not meta:
            meta = {}
        header = [
            self.protocol.value,
            self.name,
            msgpack.Timestamp.from_unix_nano(time.time_ns()),
            meta,
        ]
        return msgpack.packb(header)
