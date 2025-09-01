"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2

Module implementing the Constellation Monitoring Distribution Protocol.
"""

import io
import logging
from enum import Enum
from threading import Lock
from typing import Any

import msgpack  # type: ignore[import-untyped]
import zmq

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
        unit: str,
        handling: MetricsType,
        value: Any = None,
    ):
        self.name: str = name
        self.unit: str = unit
        self.handling: MetricsType = handling
        self.value: Any = value
        self.sender: str = ""
        self.time: msgpack.Timestamp = msgpack.Timestamp(0)
        self.meta: dict[str, Any] | None = None

    def pack(self) -> bytes:
        """Pack metric for CMDP payload."""
        stream = io.BytesIO()
        packer = msgpack.Packer()
        stream.write(packer.pack(self.value))
        stream.write(packer.pack(self.handling.value))
        stream.write(packer.pack(self.unit))
        return stream.getvalue()

    def __str__(self) -> str:
        """Convert to string."""
        t = ""
        if self.time:
            t = f" at {self.time}"
        return f"{self.name}: {self.value} [{self.unit}]{t}"


class Notification:
    """Class to hold information for a Constellation CMDP notification."""

    def __init__(
        self,
        prefix: str,
    ) -> None:
        self.sender: str = ""
        self.time: msgpack.Timestamp = msgpack.Timestamp(0)
        self.topics: dict[str, str] = {}
        self.topic_prefix: str = prefix


def decode_log(name: str, topic: str, msg: list[bytes]) -> logging.LogRecord:
    """Receive a Constellation log message."""
    # Read header
    header = MessageHeader(name, Protocol.CMDP1).decode(msg[1])
    # assert to help mypy determine len of tuple returned
    assert len(header) == 3, "Header decoding resulted in too many values for CMDP."
    sender, time, record = header
    level_name = topic.split("/")[1]
    # assert to help mypy determine type
    assert isinstance(record, dict)
    # receive payload
    record["msg"] = msg[2].decode()
    record["name"] = sender
    record["levelname"] = level_name
    record["levelno"] = logging.getLevelNamesMapping()[level_name]
    return logging.makeLogRecord(record)


def decode_metric(name: str, topic: str, msg: list[Any]) -> Metric:
    """Receive a Constellation STATS message and return a Metric."""
    name = topic.split("/")[1]
    # Read header
    header = MessageHeader(name, Protocol.CMDP1).decode(msg[1])
    # assert to help mypy determine len of tuple returned
    assert len(header) == 3, "Header decoding resulted in too many values for CMDP."
    sender, time, record = header
    # Unpack metric payload
    unpacker = msgpack.Unpacker()
    unpacker.feed(msg[2])
    value = unpacker.unpack()
    handling = unpacker.unpack()
    unit = unpacker.unpack()
    # Create metric and fill in sender
    m = Metric(name, unit, MetricsType(handling), value)
    m.sender = sender
    m.time = time
    m.meta = record
    return m


def decode_notification(name: str, topic: str, msg: list[bytes]) -> Notification:
    """Receive a Constellation log message."""
    # Read header
    header = MessageHeader(name, Protocol.CMDP1).decode(msg[1])
    # assert to help mypy determine len of tuple returned
    assert len(header) == 3, "Header decoding resulted in too many values for CMDP."
    sender, time, record = header
    unpacker = msgpack.Unpacker()
    unpacker.feed(msg[2])
    topics = unpacker.unpack()
    n = Notification(topic)
    n.sender = sender
    n.time = time
    n.topics = topics
    return n


class CMDPTransmitter:
    """Class for sending Constellation monitoring messages via ZMQ."""

    def __init__(self, name: str, socket: zmq.Socket | None):  # type: ignore[type-arg]
        """Initialize transmitter."""
        self.name = name
        self.msgheader = MessageHeader(name, Protocol.CMDP1)
        self._socket = socket
        self._lock = Lock()

    def send(self, data: logging.LogRecord | Metric) -> None:
        """Send a LogRecord or a Metric."""
        if isinstance(data, logging.LogRecord):
            self.send_log(data)
        elif isinstance(data, Metric):
            self.send_metric(data)
        else:
            raise RuntimeError(f"CMDPTransmitter cannot send object of type '{type(data)}'")

    def send_log(self, record: logging.LogRecord) -> None:
        """Send a LogRecord via an ZMQ socket.

        Follows the Constellation Monitoring Distribution Protocol.

        record: LogRecord to send.

        """
        topic = f"LOG/{record.levelname}/{record.name}"
        # Instead of just adding the formatted message, this adds key attributes
        # of the LogRecord, allowing to reconstruct the message on the other
        # end.
        meta = {
            "filename": record.filename,
            "pathname": record.pathname,
            "lineno": record.lineno,
            "funcName": record.funcName,
            "module": record.module,
            "thread": record.thread,
            "threadName": record.threadName,
            "process": record.process,
            "processName": record.processName,
            "created": record.created,
            "msecs": record.msecs,
        }
        tb: str | None = getattr(record, "traceback", None)
        if tb:
            meta["traceback"] = tb
        payload = record.getMessage().encode()
        self._dispatch(topic, payload, meta)

    def send_metric(self, metric: Metric) -> None:
        """Send a metric via a ZMQ socket."""
        topic = "STAT/" + metric.name.upper()
        payload = metric.pack()
        meta = None
        self._dispatch(topic, payload, meta)

    def recv(self, flags: int = 0) -> logging.LogRecord | Metric | Notification | None:
        """Receive a Constellation monitoring message and return log or metric."""
        if not self._socket:
            raise RuntimeError("Monitoring ZMQ socket misconfigured")
        try:
            with self._lock:
                msg = self._socket.recv_multipart(flags)
            topic = msg[0].decode("utf-8")
        except zmq.ZMQError as e:
            if "Resource temporarily unavailable" not in e.strerror:
                raise RuntimeError("CommandTransmitter encountered zmq exception") from e
            return None
        if topic.startswith("STAT/"):
            return decode_metric(self.name, topic, msg)
        elif topic.startswith("LOG/"):
            return decode_log(self.name, topic, msg)
        elif topic.startswith("LOG?") or topic.startswith("STAT?"):
            return decode_notification(self.name, topic, msg)
        else:
            raise RuntimeError(f"CMDPTransmitter cannot decode messages of topic '{topic}'")

    def closed(self) -> bool:
        """Return whether socket is closed or not."""
        if not isinstance(self._socket, zmq.Socket):
            return True
        if self._socket:
            # NOTE: according to zmq.Socket, this should return a bool; mypy thinks
            # "str | bytes | int" though..?
            return bool(self._socket.closed)
        return True

    def close(self) -> None:
        """Close the socket."""
        if not self._socket:
            raise RuntimeError("Monitoring ZMQ socket misconfigured")
        with self._lock:
            self._socket.close()
            self._socket = None

    def _dispatch(
        self,
        topic: str,
        payload: bytes,
        meta: dict[str, Any] | None = None,
        flags: int = 0,
    ) -> None:
        """Dispatch a message via ZMQ socket."""
        with self._lock:
            if not self._socket:
                # closed already
                return
            topic = topic.upper()
            flags = zmq.SNDMORE | flags
            self._socket.send_string(topic, flags)
            self.msgheader.send(self._socket, meta=meta, flags=flags)
            flags = flags & ~zmq.SNDMORE
            self._socket.send(payload, flags=flags)


class CMDPPublisher(CMDPTransmitter):
    """Class for publishing Constellation monitoring messages via ZMQ."""

    def __init__(self, name: str, socket: zmq.Socket | None):  # type: ignore[type-arg]
        """Initialize transmitter."""
        super().__init__(name, socket)
        self.log_topics: dict[str, str] = {}
        self.stat_topics: dict[str, str] = {}
        self.subscriptions: dict[str, int] = {}

    def register_log(self, topic: str, description: str | None = "") -> None:
        """Register a LOG topic that subscribers should be notified about."""
        if description is None:
            description = ""
        self.log_topics[topic.upper()] = description
        if "LOG?" in self.subscriptions.keys():
            self._send_log_notification()

    def register_stat(self, topic: str, description: str | None = "") -> None:
        """Register a STAT topic that subscribers should be notified about."""
        if description is None:
            description = ""
        self.stat_topics[topic.upper()] = description
        if "STAT?" in self.subscriptions.keys():
            self._send_stat_notification()

    def has_log_subscribers(self, record: logging.LogRecord) -> bool:
        """Return whether or not we have subscribers for the given log topic."""
        # do we have a global subscription?
        if "LOG/" in self.subscriptions.keys():
            return True
        topic = f"LOG/{record.levelname}/{record.name}"
        if topic in self.subscriptions.keys():
            return True
        # do we have a subscription to the level??
        if f"LOG/{record.levelname}" in self.subscriptions.keys():
            return True
        return False

    def has_metric_subscribers(self, metric_name: str) -> bool:
        """Return whether or not we have subscribers for the given metric data topic."""
        # do we have a global subscription?
        if "STAT/" in self.subscriptions.keys():
            return True
        topic = f"STAT/{metric_name.upper()}"
        if topic in self.subscriptions.keys():
            return True
        return False

    def update_subscriptions(self) -> None:
        """Receive a Constellation subscription message and handle that."""
        if not self._socket:
            raise RuntimeError("Monitoring ZMQ socket misconfigured")

        while True:
            try:
                with self._lock:
                    msg = self._socket.recv_multipart(flags=zmq.NOBLOCK)
                # unpack list, decode string, drop the first character
                topic = msg[0].decode()[1:]
                # First byte \x01 is subscription, \0x00 is unsubscription
                subscribe = bool(msg[0][0])
                count = self.subscriptions.get(topic, 0)
                # subscription:
                if subscribe:
                    self.subscriptions[topic] = count + 1
                else:
                    # remove key if no subscribers are left
                    if count - 1 <= 0:
                        self.subscriptions.pop(topic, None)
                    else:
                        # update count
                        self.subscriptions[topic] = count - 1
                # send current notifications if requested
                if topic.startswith("LOG?") and subscribe:
                    self._send_log_notification()
                elif topic.startswith("STAT?") and subscribe:
                    self._send_stat_notification()
                # TODO warn about ignored messages
            except zmq.ZMQError as e:
                if "Resource temporarily unavailable" not in e.strerror:
                    raise RuntimeError("CMDPPublisher encountered ZMQ exception") from e
                break

    def _send_log_notification(self) -> None:
        stream = io.BytesIO()
        packer = msgpack.Packer()
        stream.write(packer.pack(self.log_topics))
        self._dispatch("LOG?", payload=stream.getvalue())

    def _send_stat_notification(self) -> None:
        stream = io.BytesIO()
        packer = msgpack.Packer()
        stream.write(packer.pack(self.stat_topics))
        self._dispatch("STAT?", payload=stream.getvalue())
