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
        self,
        name: str,
        description: str,
        unit: str,
        handling: MetricsType,
        value: any = None,
    ):
        self.name = name
        self.description = description
        self.unit = unit
        self.handling = handling
        self.value = value
        self.sender = None
        self.time = None
        self.meta = None

    def as_list(self):
        """Convert metric to list."""
        return [self.description, self.unit, self.handling.value, self.value]

    def __str__(self):
        """Convert to string."""
        t = ""
        if self.time:
            t = f" at {self.time}"
        return f"{self.name}: {self.value} [{self.unit}]{t}"


class CMDPTransmitter:
    """Class for sending Constellation monitoring messages via ZMQ."""

    def __init__(self, name: str, socket: zmq.Socket):
        """Initialize transmitter."""
        self.name = name
        self.msgheader = MessageHeader(name, Protocol.CMDP)
        self._socket = socket

    def send(self, data: logging.LogRecord | Metric):
        if isinstance(data, logging.LogRecord):
            return self.send_log(data)
        elif isinstance(data, Metric):
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
        payload = record.getMessage()
        return self._dispatch(topic, payload, meta)

    def send_metric(self, metric: Metric):
        """Send a metric via a ZMQ socket."""
        topic = "STATS/" + metric.name
        payload = metric.as_list()
        meta = None
        return self._dispatch(topic, payload, meta)

    def recv(self, flags=0) -> logging.LogRecord | Metric | None:
        """Receive a Constellation monitoring message and return log or metric."""
        try:
            topic = self._socket.recv(flags).decode("utf-8")
        except zmq.ZMQError:
            return None
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
        sender, time, record = self.msgheader.recv(self._socket)
        # receive payload
        message = msgpack.unpackb(self._socket.recv())
        # message == msg % args
        if "msg" not in record and "args" not in record:
            record["msg"] = message
        record["created"] = time.to_datetime().timestamp()
        record["name"] = sender
        record["levelname"] = topic.split("/")[1]
        return logging.makeLogRecord(record)

    def _recv_metric(self, topic) -> Metric:
        """Receive a Constellation STATS message and return a Metric."""
        name = topic.split("/")[1]
        # Read header
        sender, time, record = self.msgheader.recv(self._socket)
        description, unit, handling, value = msgpack.unpackb(self._socket.recv())
        m = Metric(name, description, unit, MetricsType(handling), value)
        m.sender = sender
        m.time = time
        m.meta = record
        return m

    def _dispatch(
        self,
        topic: str,
        payload: any,
        meta: dict = None,
        flags: int = 0,
    ):
        """Dispatch a message via ZMQ socket."""
        flags = zmq.SNDMORE | flags
        self._socket.send_string(topic, flags)
        self.msgheader.send(self._socket, meta=meta, flags=flags)
        flags = flags & ~zmq.SNDMORE
        self._socket.send(msgpack.packb(payload), flags=flags)