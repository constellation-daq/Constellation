#!/usr/bin/env python3
"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0

Module implementing the Constellation Data Transmission Protocol.
"""

import time
from enum import Enum

import msgpack
import zmq

from .protocol import MessageHeader, Protocol


class CDTPMessageIdentifier(Enum):
    """Defines the message types of the CDTP.

    Part of the Constellation Satellite Data Protocol, see
    docs/protocols/cdtp.md for details.

    """

    DAT = 0x0
    BOR = 0x1
    EOR = 0x2


class CDTPMessage:
    """Class holding details of a received CDTP command."""

    name: str = None
    timestamp: msgpack.Timestamp = None
    msgtype: CDTPMessageIdentifier = None
    sequence_number: int = None
    meta: dict[str, any]
    payload: any = None

    def set_header(self, name, timestamp, msgtype, sequence_number, time_sequence):
        """Sets information retrieved from a message header."""
        self.name = name
        self.timestamp = timestamp
        self.msgtype = msgtype
        self.sequence_number = sequence_number
        self.time_sequence = time_sequence


class DataTransmitter:
    """Base class for sending Constellation data packets via ZMQ."""

    def __init__(self, name: str, socket: zmq.Socket):
        """Initialize transmitter.

        socket: the ZMQ socket to use if no other is specified on send()/recv()
        calls.

        host: the name to use in the message header. Defaults to system host name.

        """
        self.name = name
        self.msgheader = MessageHeader(name, Protocol.CDTP)
        self._socket = socket

    def send_start(self, payload, meta, flags: int = 0):
        self._sequence_number = 0
        return self._dispatch(payload, CDTPMessageIdentifier.BOR, meta, flags)

    def send_data(self, payload, meta, flags: int = 0):
        self._sequence_number += 1
        return self._dispatch(payload, CDTPMessageIdentifier.DAT, meta, flags)

    def send_end(self, payload, meta, flags: int = 0):
        return self._dispatch(payload, CDTPMessageIdentifier.EOR, meta, flags)

    def _dispatch(
        self,
        payload,
        run_identifier: CDTPMessageIdentifier,
        meta: dict = None,
        socket: zmq.Socket = None,
        flags: int = 0,
    ):
        """Send a payload over a ZMQ socket.

        Follows the Constellation Data Transmission Protocol.

        payload: data to send.

        meta: dictionary to include in the map of the message header.

        socket: ZMQ socket to use for transmission. If none is specified, use
        the one the class was initialized with.

        flags: additional ZMQ socket flags to use during transmission.

        Returns: return of socket.send(payload) call.

        """
        if not meta:
            meta = {}
        # use default socket if none was specified
        if not socket:
            socket = self._socket
        flags = zmq.SNDMORE | flags
        # message header
        self.msgheader.send(socket, flags=flags)
        socket.send(msgpack.packb(Protocol.CDTP), flags=flags)
        socket.send(msgpack.packb(self.name), flags=flags)
        socket.send(msgpack.packb(time.time_ns()), flags=flags)
        socket.send(msgpack.packb(run_identifier))
        socket.send(msgpack.packb(self.sequence_number))
        self.sequence_number += 1
        socket.send(msgpack.packb(meta), flags=flags)
        # payload
        flags = flags & (~zmq.SNDMORE)  # flip SNDMORE bit
        return socket.send(msgpack.packb(payload), flags=flags)

    def recv(self, socket: zmq.Socket = None, flags: int = 0):
        """Receive a multi-part data transmission.

        Follows the Constellation Data Transmission Protocol.

        socket: ZMQ socket to use for transmission. If none is specified, use
        the one the class was initialized with.

        flags: additional ZMQ socket flags to use during transmission.

        Returns: payload, map (meta data), timestamp and sending host.

        """
        # use default socket if none was specified
        if not socket:
            socket = self._socket
        msg = socket.recv_multipart(flags=flags)
        if not len(msg) == 6:
            raise RuntimeError(
                f"Received message with wrong length of {len(msg)} parts!"
            )
        if not msgpack.unpackb(msg[0]) == Protocol.CDTP:
            raise RuntimeError(
                f"Received message with malformed CDTP header: {msgpack.unpackb(msg[0])}!"
            )
        payload = msgpack.unpackb(msg[5])
        run_seq = msgpack.unpackb(msg[4])
        run_id = msgpack.unpackb(msg[3])
        ts = msgpack.unpackb(msg[2])
        host = msgpack.unpackb(msg[1])
        return payload, run_seq, run_id, host, ts
