#!/usr/bin/env python3
"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0

Module implementing the Constellation Monitoring Distribution Protocol.
"""

import msgpack
import zmq
import logging
from enum import Enum

from .protocol import MessageHeader, Protocol


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

    def as_list(self):
        """Convert metric to list."""
        return [self.description, self.unit, self.handling.value, self.value]


class CMDPTransmitter:
    """Class for sending Constellation monitoring messages via ZMQ."""

    def __init__(self, name: str, socket: zmq.Socket):
        """Initialize transmitter."""
        self.name = name
        self.msgheader = MessageHeader(name, Protocol.CMDP)
        self._socket = socket

    def send(self, data: logging.LogRecord | Metric):
        if isinstance(logging.LogRecord, data):
            return self.send_log(data)
        elif isinstance(Metric, data):
            return self.send_metric(data)
        else:
            raise RuntimeError(
                f"CMDPTransmitter cannot send object of type '{type(data)}'"
            )

    def send_log(self, record: logging.LogRecord):
        """Send a LogRecord via an ZMQ socket.

        Follows the Constellation Monitoring Distribution Protocol.

        record: LogRecord to send.

        Returns: return of zmq.Socket.send() call.

        """
        topic = f"LOG/{record.levelname}/{record.name}"
        # Instead of just adding the formatted message, this adds key attributes
        # of the LogRecord, allowing to reconstruct the message on the other
        # end.
        meta = {
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
        }
        # the formatted message:
        payload = record.message
        return self._dispatch(topic, payload, meta)

    def send_metric(self, metric: Metric):
        """Send a metric via a ZMQ socket."""
        topic = "STATS/" + metric.name
        payload = metric.as_list()
        meta = None
        return self._dispatch(topic, payload, meta)

    def recv(self) -> logging.LogRecord | Metric:
        """Receive a Constellation monitoring message and return log or metric."""
        topic = self.socket.recv().decode("utf-8")
        if topic.startswith("STATS/"):
            return self._recv_metric(topic)
        elif topic.startswith("LOG/"):
            return self._recv_log(topic)
        else:
            raise RuntimeError(
                f"CMDPTransmitter cannot decode messages of topic '{topic}'"
            )

    def closed(self) -> bool:
        """Return whether socket is closed or not."""
        if self._socket:
            return self._socket.closed
        return True

    def close(self):
        """Close the socket."""
        self._socket.close()

    def _recv_log(self, topic: str) -> logging.LogRecord:
        """Receive a Constellation log message."""

        # Read header
        unpacker_header = msgpack.Unpacker()
        unpacker_header.feed(self.socket.recv())
        protocol = unpacker_header.unpack()
        if not protocol == Protocol.CMDP:
            raise RuntimeError(
                f"Received message with malformed CMDP header: {protocol}!"
            )
        sender = unpacker_header.unpack()
        time = unpacker_header.unpack()
        record = unpacker_header.unpack()
        if "msg" not in record and "args" not in record:
            record["msg"] = self.socket.recv().decode("utf-8")
        record["created"] = time.to_datetime().timestamp()
        record["name"] = sender
        record["levelname"] = topic.split("/")[1]
        return logging.makeLogRecord(record)

    def _recv_metric(self, topic) -> Metric:
        """Receive a Constellation STATS message and return a Metric."""
        name = topic.split("/")[1]
        header = msgpack.unpackb(self.queue.recv())
        if not header[0] == Protocol.CMDP:
            raise RuntimeError(
                f"Received message with malformed CDTP header: '{header}'!"
            )
        description, unit, handling, value = msgpack.unpackb(self.queue.recv())
        return Metric(name, description, unit, MetricsType(handling), value)

    def _dispatch(
        self,
        topic: str,
        payload: any,
        meta: dict = None,
        flags: int = 0,
    ):
        """Dispatch a message via ZMQ socket."""
        flags = zmq.SNDMORE | flags
        self.socket.send_string(topic, flags)
        self.msgheader.send(self.socket, meta=meta, flags=flags)
        self.socket.send(msgpack.packb(payload), flags=zmq.NOBLOCK)
