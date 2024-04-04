#!/usr/bin/env python3
"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0

Module implementing the Constellation Data Transmission Protocol.
"""

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

    def set_header(self, name, timestamp, meta):
        """Sets information retrieved from a message header."""
        self.name = name
        self.timestamp = timestamp
        self.meta = meta

    def __str__(self):
        """Pretty-print request."""
        s = "Data message from {} received at {} of type {}, "
        s += "sequence number {} {} payload and meta {}."
        return s.format(
            self.name,
            self.timestamp,
            self.msgtype,
            self.sequence_number,
            "with a" if self.payload else "without a",
            self.meta,
        )


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
        self.running = False

    def send_start(self, payload, meta: dict = {}, flags: int = 0):
        self.sequence_number = 0
        self.running = True
        return self._dispatch(payload, CDTPMessageIdentifier.BOR, meta, flags)

    def send_data(self, payload, meta: dict = {}, flags: int = 0):
        if self.running:
            self.sequence_number += 1
            return self._dispatch(payload, CDTPMessageIdentifier.DAT, meta, flags)
        msg = "Data transfer sequence not started"
        raise RuntimeError(msg)

    def send_end(self, payload, meta: dict = {}, flags: int = 0):
        self.running = False
        return self._dispatch(payload, CDTPMessageIdentifier.EOR, meta, flags)

    def _dispatch(
        self,
        payload,
        run_identifier: CDTPMessageIdentifier,
        meta: dict = {},
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

        # Set option to send more based on flag input
        flags = zmq.SNDMORE | flags
        # message header
        self.msgheader.send(self._socket, meta=meta, flags=flags)
        self._socket.send([run_identifier, self.sequence_number], flags=flags)

        # payload
        flags = flags & (~zmq.SNDMORE)  # flip SNDMORE bit
        return self._socket.send(msgpack.packb(payload), flags=flags)

    def recv(self, flags: int = 0) -> CDTPMessage:
        """Receive a multi-part data transmission.

        Follows the Constellation Data Transmission Protocol.

        socket: ZMQ socket to use for transmission. If none is specified, use
        the one the class was initialized with.

        flags: additional ZMQ socket flags to use during transmission.

        Returns: payload, map (meta data), timestamp and sending host.

        """
        try:
            datamsg = self._socket.recv_multipart(flags=flags)
        except zmq.ZMQError:
            return None
        msg = CDTPMessage()
        msg.set_header(*self.msgheader.decode(datamsg[0]))
        msg.msgtype, msg.sequence_number = msgpack.unpackb(datamsg[1])
        msg.payload = msgpack.unpackb(datamsg[2])
        return msg
