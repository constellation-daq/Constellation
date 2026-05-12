"""
SPDX-FileCopyrightText: 2025 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2

Provides message class for CMDP1
"""

from __future__ import annotations

from dataclasses import dataclass
from datetime import datetime
from io import BytesIO
from logging import LogRecord, makeLogRecord
from typing import Any, cast

import msgpack  # type: ignore[import-untyped]

from ..protocol import Protocol
from ..protocol.cmdp1 import LogLevel, Metric
from .exceptions import InvalidProtocolError, MessageDecodingError, UnexpectedProtocolError
from .msgpack_helpers import msgpack_pack, msgpack_unpack_to
from .multipart import MultipartMessage


class CMDP1Message:
    """Message class for CMDP1"""

    def __init__(
        self,
        sender: str,
        topic: str,
        payload: bytes,
        time: datetime | None = None,
        tags: dict[str, Any] | None = None,
    ):
        self._protocol = Protocol.CMDP1
        self._sender = sender
        self._time = time if time is not None else datetime.now().astimezone()
        self._tags = tags if tags is not None else {}
        self._topic = topic
        self._payload = payload

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
    def cmdp_topic(self) -> str:
        return self._topic

    def is_log_message(self) -> bool:
        return self._topic.startswith("LOG/")

    def is_stat_message(self) -> bool:
        return self._topic.startswith("STAT/")

    def is_notification(self) -> bool:
        return self._topic.startswith("LOG?") or self._topic.startswith("STAT?")

    def assemble(self) -> MultipartMessage:
        streams: list[BytesIO | bytes] = []
        packer = msgpack.Packer(datetime=True)

        # Pack topic
        topic_stream = BytesIO()
        topic_stream.write(self._topic.encode())
        streams.append(topic_stream)
        # Pack header
        header_stream = BytesIO()
        header_stream.write(packer.pack(self._protocol.value))
        header_stream.write(packer.pack(self._sender))
        header_stream.write(packer.pack(self._time))
        header_stream.write(packer.pack(self._tags))
        streams.append(header_stream)
        # Add payload
        streams.append(self._payload)

        return MultipartMessage(streams)

    @staticmethod
    def disassemble(frames: list[bytes]) -> CMDP1Message:
        unpacker = msgpack.Unpacker(timestamp=3)
        if len(frames) != 3:
            raise MessageDecodingError(f"Expected 3 frames, got {len(frames)}")

        # Unpack topic
        topic = frames[0].decode()
        if not topic.startswith(("LOG/", "STAT/", "LOG?", "STAT?")):
            raise MessageDecodingError(f"Invalid message topic `{topic}`, neither log nor telemetry message")
        # Unpack header
        unpacker.feed(frames[1])
        protocol = msgpack_unpack_to(unpacker, str)
        try:
            protocol = Protocol(protocol)
        except ValueError as e:
            raise InvalidProtocolError(protocol) from e
        if protocol is not Protocol.CMDP1:
            raise UnexpectedProtocolError(protocol, Protocol.CMDP1)
        sender = msgpack_unpack_to(unpacker, str)
        time = msgpack_unpack_to(unpacker, datetime)
        tags = msgpack_unpack_to(unpacker, dict)
        # Store payload
        payload = frames[2]

        # Assemble and return message
        return CMDP1Message(sender, topic, payload, time, tags)

    def __str__(self) -> str:
        return f"CMDP1 message with topic `{self._topic}` from {self._sender} received at {self._time}"


def _split_log_topic(topic: str) -> tuple[LogLevel, str]:
    level_endpos = topic.find("/", 4)
    level_str = topic[4 : level_endpos if level_endpos != -1 else None]
    log_topic = topic[level_endpos + 1 :] if level_endpos != -1 else ""
    try:
        return LogLevel[level_str.upper()], log_topic
    except KeyError as e:
        raise MessageDecodingError(f"`{level_str}` is not a valid log level") from e


class CMDP1LogMessage(CMDP1Message):
    """Message class for CMDP1 log messages"""

    def __init__(
        self,
        sender: str,
        log_level: LogLevel,
        log_topic: str,
        log_message: str,
        time: datetime | None = None,
        tags: dict[str, Any] | None = None,
    ):
        self._log_level = log_level
        self._log_topic = log_topic
        self._log_message = log_message
        super().__init__(
            sender,
            f"LOG/{self._log_level.name}{'/' + self._log_topic if self._log_topic else ''}",
            self._log_message.encode(),
            time,
            tags,
        )

    @property
    def log_level(self) -> LogLevel:
        return self._log_level

    @property
    def log_topic(self) -> str:
        return self._log_topic

    @property
    def log_message(self) -> str:
        return self._log_message

    def to_log_record(self) -> LogRecord:
        meta: dict[str, Any] = {}
        meta["name"] = self._log_topic
        meta["levelname"] = self._log_level.name
        meta["levelno"] = self._log_level.value
        meta["msg"] = self._log_message
        meta["created"] = self._time.timestamp()
        meta["sender"] = self._sender
        meta.update(self.tags)
        return makeLogRecord(meta)

    @staticmethod
    def from_log_record(record: LogRecord, sender: str = "") -> CMDP1LogMessage:
        tags = {
            "filename": record.filename,
            "pathname": record.pathname,
            "lineno": record.lineno,
            "funcName": record.funcName,
            "module": record.module,
            "thread": record.thread,
            "threadName": record.threadName,
            "process": record.process,
            "processName": record.processName,
        }
        tb: str | None = getattr(record, "traceback", None)
        if tb:
            tags["traceback"] = tb
        return CMDP1LogMessage(
            sender,
            LogLevel(record.levelno),
            record.name.upper(),
            record.getMessage(),
            datetime.fromtimestamp(record.created).astimezone(),
            tags,
        )

    @staticmethod
    def from_cmdp_message(msg: CMDP1Message) -> CMDP1LogMessage:
        if not msg.is_log_message():
            raise MessageDecodingError("Not a log message")
        # Cast to CMDP1LogMessage
        msg.__class__ = CMDP1LogMessage
        msg = cast(CMDP1LogMessage, msg)
        # Unpack log level, log topic and log message
        msg._log_level, msg._log_topic = _split_log_topic(msg._topic)
        msg._log_message = msg._payload.decode()

        return msg

    @staticmethod
    def disassemble(frames: list[bytes]) -> CMDP1LogMessage:
        return CMDP1LogMessage.from_cmdp_message(CMDP1Message.disassemble(frames))


@dataclass
class MetricValue:
    metric: Metric
    value: Any

    def assemble(self) -> bytes:
        packer = msgpack.Packer()
        stream = BytesIO()
        stream.write(packer.pack(self.value))
        stream.write(packer.pack(0x0))  # unused metric flag
        stream.write(packer.pack(self.metric.unit))
        return stream.getvalue()

    @staticmethod
    def disassemble(name: str, frame: bytes) -> MetricValue:
        unpacker = msgpack.Unpacker()
        unpacker.feed(frame)
        value = unpacker.unpack()
        _ = unpacker.unpack()  # unused metric flag
        unit = msgpack_unpack_to(unpacker, str)
        return MetricValue(Metric(name, unit, ""), value)


class CMDP1StatMessage(CMDP1Message):
    """Message class for CMDP1 stat messages"""

    def __init__(
        self,
        sender: str,
        metric: Metric,
        value: Any,
        time: datetime | None = None,
        tags: dict[str, Any] | None = None,
    ):
        self._metric_value = MetricValue(metric, value)
        super().__init__(sender, f"STAT/{metric.name.upper()}", self._metric_value.assemble(), time, tags)

    @property
    def metric(self) -> Metric:
        return self._metric_value.metric

    @property
    def value(self) -> Any:
        return self._metric_value.value

    @staticmethod
    def from_cmdp_message(msg: CMDP1Message) -> CMDP1StatMessage:
        if not msg.is_stat_message():
            raise MessageDecodingError("Not a stat message")
        # Cast to CMDP1StatMessage
        msg.__class__ = CMDP1StatMessage
        msg = cast(CMDP1StatMessage, msg)
        # Unpack metric value
        metric_name = msg._topic[5:]
        msg._metric_value = MetricValue.disassemble(metric_name, msg._payload)

        return msg

    @staticmethod
    def disassemble(frames: list[bytes]) -> CMDP1StatMessage:
        return CMDP1StatMessage.from_cmdp_message(CMDP1Message.disassemble(frames))


class CMDP1Notification(CMDP1Message):
    """Message class for CMDP1 notifications"""

    def __init__(
        self,
        sender: str,
        topics_prefix: str,
        topics: dict[str, str],
        time: datetime | None = None,
        tags: dict[str, Any] | None = None,
    ):
        self._topics = topics
        super().__init__(sender, topics_prefix + "?", msgpack_pack(self._topics), time, tags)

    @property
    def topics_prefix(self) -> str:
        return self._topic[:-1]

    @property
    def topics(self) -> dict[str, str]:
        return self._topics

    @staticmethod
    def from_cmdp_message(msg: CMDP1Message) -> CMDP1Notification:
        if not msg.is_notification():
            raise MessageDecodingError("Not a notification")
        # Cast to CMDP1Notification
        msg.__class__ = CMDP1Notification
        msg = cast(CMDP1Notification, msg)
        # Unpack topics
        unpacker = msgpack.Unpacker()
        unpacker.feed(msg._payload)
        msg._topics = msgpack_unpack_to(unpacker, dict)

        return msg

    @staticmethod
    def disassemble(frames: list[bytes]) -> CMDP1Notification:
        return CMDP1Notification.from_cmdp_message(CMDP1Message.disassemble(frames))
