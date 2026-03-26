"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2

Module implementing the Constellation Monitoring Distribution Protocol
"""

import logging
from datetime import datetime
from threading import Lock
from typing import Any

import zmq

from .message.cmdp1 import CMDP1LogMessage, CMDP1Message, CMDP1Notification, CMDP1StatMessage
from .protocol.cmdp1 import Metric


class CMDPTransmitter:
    """Class for sending monitoring information via CMDP"""

    def __init__(self, sender: str, socket: zmq.Socket):
        self._sender = sender
        self._socket = socket
        self._lock = Lock()

    def send_log(self, record: logging.LogRecord) -> None:
        """Send a log record"""
        self._send_message(CMDP1LogMessage.from_log_record(record, self._sender))

    def send_metric(self, metric: Metric, value: Any) -> None:
        """Send a metric"""
        time_now = datetime.now().astimezone()
        msg = CMDP1StatMessage(self._sender, metric, value, time_now)
        self._send_message(msg)

    def send_notification(self, topics_prefix: str, topics: dict[str, str]) -> None:
        msg = CMDP1Notification(self._sender, topics_prefix, topics)
        self._send_message(msg)

    def closed(self) -> bool:
        """Return whether socket is closed or not"""
        with self._lock:
            return bool(self._socket.closed)

    def close(self) -> None:
        """Close the socket"""
        with self._lock:
            if not bool(self._socket.closed):
                self._socket.close()

    def _send_message(
        self,
        msg: CMDP1Message,
        flags: int = 0,
    ) -> None:
        with self._lock:
            if not self._socket:
                return  # closed already
            msg.assemble().send(self._socket, flags)


class CMDPPublisher(CMDPTransmitter):
    """Class for sending and publishing monitoring information"""

    def __init__(self, name: str, socket: zmq.Socket):
        """Initialize transmitter."""
        super().__init__(name, socket)
        self.log_topics: dict[str, str] = {}
        self.stat_topics: dict[str, str] = {}
        self.subscriptions: dict[str, int] = {}

    def register_log(self, topic: str, description: str | None = None) -> None:
        """Register a LOG topic that subscribers should be notified about."""
        if description is None:
            description = ""
        self.log_topics[topic.upper()] = description
        if "LOG?" in self.subscriptions:
            self.send_notification("LOG", self.log_topics)

    def register_stat(self, topic: str, description: str) -> None:
        """Register a STAT topic that subscribers should be notified about."""
        self.stat_topics[topic.upper()] = description
        if "STAT?" in self.subscriptions:
            self.send_notification("STAT", self.stat_topics)

    def unregister_stat(self, topic: str) -> None:
        popped = self.stat_topics.pop(topic.upper(), None)
        if popped is not None and "STAT?" in self.subscriptions:
            self.send_notification("STAT", self.stat_topics)

    def has_log_subscribers(self, levelname: str, topic: str | None = None):
        """Return whether or not there are subscribers for the given log level and topic."""
        # Check global subscription
        if "LOG/" in self.subscriptions:
            return True
        # Check level subscription
        if f"LOG/{levelname}" in self.subscriptions:
            return True
        # Check topic subscription
        if topic is not None and f"LOG/{levelname}/{topic}" in self.subscriptions:
            return True
        return False

    def has_log_subscribers_record(self, record: logging.LogRecord) -> bool:
        """Return whether or not we have subscribers for the given log topic."""
        return self.has_log_subscribers(record.levelname, record.name)

    def has_metric_subscribers(self, metric_name: str) -> bool:
        """Return whether or not we have subscribers for the given metric data topic."""
        # Check global subscription
        if "STAT/" in self.subscriptions:
            return True
        # Check topic subscription
        if f"STAT/{metric_name.upper()}" in self.subscriptions:
            return True
        return False

    def update_subscriptions(self) -> None:
        """Receive and handle a subscription messages"""

        # Run until there are no more messages to process:
        while True:
            try:
                with self._lock:
                    frames = self._socket.recv_multipart(flags=zmq.NOBLOCK)
                msg = frames[0]
                # unpack list, decode string, drop the first character
                topic = msg.decode()[1:]
                # First byte \x01 is subscription, \0x00 is unsubscription
                subscribe = bool(msg[0])
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
                    self.send_notification("LOG", self.log_topics)
                elif topic.startswith("STAT?") and subscribe:
                    self.send_notification("STAT", self.stat_topics)
                else:
                    if not topic.startswith("STAT") and not topic.startswith("LOG"):
                        raise ValueError(f"Unknown topic '{topic}'")
            except zmq.ZMQError as e:
                if "Resource temporarily unavailable" not in e.strerror:
                    raise RuntimeError("CMDPPublisher encountered ZMQ exception") from e
                break
