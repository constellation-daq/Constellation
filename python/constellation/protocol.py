#!/usr/bin/env python3
"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0

Module implementing the Constellation communication protocols.
"""

import msgpack
import zmq

import time
import platform
import logging
from enum import Enum, StrEnum


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
        return self.decode(self.queue.recv())

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


class DataTransmitter:
    """Base class for sending Constellation data packets via ZMQ."""

    def __init__(self, socket: zmq.Socket = None, host: str = None):
        """Initialize transmitter.

        socket: the ZMQ socket to use if no other is specified on send()/recv()
        calls.

        host: the name to use in the message header. Defaults to system host name.

        """
        if not host:
            host = platform.node()
        self.host = host
        self._socket = socket

    def send(
        self, payload, meta: dict = None, socket: zmq.Socket = None, flags: int = 0
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
        socket.send(msgpack.packb(Protocol.CDTP), flags=flags)
        socket.send(msgpack.packb(self.host), flags=flags)
        socket.send(msgpack.packb(time.time_ns()), flags=flags)
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
        if not len(msg) == 5:
            raise RuntimeError(
                f"Received message with wrong length of {len(msg)} parts!"
            )
        if not msgpack.unpackb(msg[0]) == Protocol.CDTP:
            raise RuntimeError(
                f"Received message with malformed CDTP header: {msgpack.unpackb(msg[0])}!"
            )
        payload = msgpack.unpackb(msg[4])
        meta = msgpack.unpackb(msg[3])
        ts = msgpack.unpackb(msg[2])
        host = msgpack.unpackb(msg[1])
        return payload, meta, host, ts

    def close(self):
        """Close the socket."""
        self._socket.close()


class LogTransmitter:
    """Class for sending Constellation log messages via ZMQ."""

    def __init__(self, socket: zmq.Socket = None, host: str = None):
        """Initialize transmitter.

        socket: the ZMQ socket to use if no other is specified on send()/recv()
        calls.

        host: the name to use in the message header. Defaults to system host name.

        """
        if not host:
            host = platform.node()
        self.host = host
        self._socket = socket

    def send(self, record: logging.LogRecord, socket: zmq.Socket = None):
        """Send a LogRecord via an ZMQ socket.

        Follows the Constellation Monitoring Distribution Protocol.

        record: LogRecord to send.

        socket: ZMQ socket to use for transmission. If none is specified, use
        the one the class was initialized with.

        Returns: return of socket.send(record) call.

        """
        # use default socket if none was specified
        if not socket:
            socket = self._socket
        topic = f"LOG/{record.levelname}/{record.name}"
        header = [
            Protocol.CMDP,
            self.host,
            msgpack.Timestamp.from_unix_nano(time.time_ns()),
            {},
        ]
        socket.send_string(topic, zmq.SNDMORE)
        socket.send(msgpack.packb(header), zmq.SNDMORE)
        # Instead of just adding the formatted message, this adds key attributes
        # of the LogRecord, allowing to reconstruct the full message on the
        # other end.
        # TODO filter and name these according to the Constellation LOG specifications
        return socket.send(
            msgpack.packb(
                {
                    "name": record.name,
                    "msg": record.msg,
                    "args": record.args,
                    "levelname": record.levelname,
                    "levelno": record.levelno,
                    "pathname": record.pathname,
                    "filename": record.filename,
                    "module": record.module,
                    "exc_info": record.exc_info,
                    "exc_text": record.exc_text,
                    "stack_info": record.stack_info,
                    "lineno": record.lineno,
                    "funcName": record.funcName,
                    "created": record.created,
                    "msecs": record.msecs,
                    "relativeCreated": record.relativeCreated,
                    "thread": record.thread,
                    "threadName": record.threadName,
                    "processName": record.processName,
                    "process": record.process,
                    "message": record.message,
                }
            )
        )

    def recv(self, socket: zmq.Socket = None):
        """Receive a Constellation log message and return a LogRecord.

        Follows the Constellation Monitoring Distribution Protocol.

        socket: ZMQ socket to use for transmission. If none is specified, use
        the one the class was initialized with.

        Returns: LogRecord.

        """
        # use default socket if none was specified
        if not socket:
            socket = self._socket
        topic = socket.recv().decode("utf-8")
        # NOTE it is currently not possible to receive both STATS and LOG on the
        # same socket as the respective classes do not handle the other.
        # TODO : refactorize LogTransmitter to allow to transparently receive logs and stats
        if not topic.startswith("LOG/"):
            raise RuntimeError(
                f"LogTransmitter cannot decode messages of topic '{topic}'"
            )

        # Read header
        unpacker_header = msgpack.Unpacker()
        unpacker_header.feed(socket.recv())
        protocol = unpacker_header.unpack()
        if not protocol == Protocol.CMDP:
            raise RuntimeError(
                f"Received message with malformed CMDP header: {protocol}!"
            )
        sender = unpacker_header.unpack()
        time = unpacker_header.unpack()
        record = unpacker_header.unpack()
        record["msg"] = socket.recv().decode("utf-8")
        record["created"] = time.to_datetime().timestamp()
        record["name"] = sender
        record["levelname"] = topic.split("/")[1]
        return logging.makeLogRecord(record)

    def closed(self) -> bool:
        """Return whether socket is closed or not."""
        if self._socket:
            return self._socket.closed
        return True

    def close(self):
        """Close the socket."""
        self._socket.close()


class MetricsType(Enum):
    LAST_VALUE = 0x1
    ACCUMULATE = 0x2
    AVERAGE = 0x3
    RATE = 0x4


class Metric:
    """Class to hold information for a Constellation metric."""

    def __init__(
        self, name: str, description: str, unit: str, handling: MetricsType, value=None
    ):
        self.name = name
        self.description = description
        self.unit = unit
        self.handling = handling
        self.value = value


class MetricsTransmitter:
    """Class to send and receive a Constellation Metric."""

    def __init__(self, socket: zmq.Socket = None, host: str = None) -> None:
        """Initialize transmitter.

        socket: the ZMQ socket to use if no other is specified on send()/recv()
        calls.

        host: the name to use in the message header. Defaults to system host name.

        """
        if not host:
            host = platform.node()
        self.host = host
        self._socket = socket

    def send(self, metric: Metric, socket: zmq.Socket = None):
        """Send a metric via a ZMQ socket.

        Follows the Constellation Monitoring Distribution Protocol.

        metric: Metric to send.

        socket: ZMQ socket to use for transmission. If none is specified, use
        the one the class was initialized with.

        Returns: return of socket.send(metric) call.

        """
        # use default socket if none was specified
        if not socket:
            socket = self._socket
        topic = "STATS/" + metric.name
        header = [
            Protocol.CMDP,
            self.host,
            msgpack.Timestamp.from_unix_nano(time.time_ns()),
            {},
        ]
        self._socket.send_string(topic, zmq.SNDMORE)
        self.queue.send(msgpack.packb(header), zmq.SNDMORE)
        self.queue.send(
            msgpack.packb(
                [metric.description, metric.unit, metric.handling.value, metric.value]
            )
        )

    def recv(self, socket: zmq.Socket = None):
        """Receive a Constellation STATS message and return a Metric.

        Follows the Constellation Monitoring Distribution Protocol.

        socket: ZMQ socket to use for transmission. If none is specified, use
        the one the class was initialized with.

        Returns: Metric.

        """
        # use default socket if none was specified
        if not socket:
            socket = self._socket
        topic = msgpack.unpackb(self.queue.recv())
        # NOTE it is currently not possible to receive both STATS and LOG on the
        # same socket as the respective classes do not handle the other.
        # TODO : refactorize MetricsTransmitter to allow to transparently receive logs and stats
        if not topic.startswith("STATS/"):
            raise RuntimeError(
                f"MetricsTransmitter cannot decode messages of topic '{topic}'"
            )
        name = topic.split("/")[1]
        header = msgpack.unpackb(self.queue.recv())
        if not header[0] == Protocol.CMDP:
            raise RuntimeError(
                f"Received message with malformed CDTP header: '{header}'!"
            )
        description, unit, handling, value = msgpack.unpackb(self.queue.recv())
        return Metric(name, description, unit, MetricsType(handling), value)

    def close(self):
        """Close the socket."""
        self._socket.close()
