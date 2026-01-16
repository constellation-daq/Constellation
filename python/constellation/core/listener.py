"""
SPDX-FileCopyrightText: 2025 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2

Module implementing listeners
"""

import logging
import time
from datetime import datetime
from logging.handlers import QueueListener
from queue import Empty, SimpleQueue
from threading import Lock, Thread
from typing import Any, cast

import zmq

from .chirp import CHIRPServiceIdentifier
from .chirpmanager import CHIRPManager, DiscoveredService, chirp_callback
from .cmdp import Metric
from .message.cmdp1 import CMDP1LogMessage, CMDP1Message, CMDP1Notification, CMDP1StatMessage
from .message.exceptions import MessageDecodingError
from .pools import SubscriberPool


class LogPoolQueue(SubscriberPool, SimpleQueue[logging.LogRecord]):
    """Simple wrapper around subscriper pool which queues CMDP message as log records

    Note: this class does not handle adding connections and subscribing to them.
    """

    def __init__(self, context: zmq.Context):
        super().__init__(context, self._receive)

    def _receive(self, frames: list[bytes]) -> None:
        try:
            self.put_nowait(CMDP1LogMessage.disassemble(frames).to_log_record())
        except MessageDecodingError:
            pass


class LogPoolQueueListener(QueueListener):
    """Simple queue listener for LogPoolQueue

    Note: this class does not handle adding connections and subscribing to them.
    """

    def __init__(self, context: zmq.Context, *args: Any, **kwargs: Any):
        super().__init__(LogPoolQueue(context), *args, **kwargs)

    @property
    def log_receiver_queue(self) -> LogPoolQueue:
        return cast(LogPoolQueue, self.queue)

    def start(self) -> None:
        super().start()
        self.log_receiver_queue.start_poll_thread()

    def stop(self) -> None:
        self.log_receiver_queue.stop_poll_thread()
        super().stop()


class MonitoringListener(CHIRPManager):
    def __init__(self, name: str, group: str, interface: list[str] | None, **kwds: Any):
        super().__init__(name=name, group=group, interface=interface, **kwds)

        self.log_cmdp_l = self.get_logger("MNTR")

        self._topics: list[str] = []
        self._topics_lock = Lock()

        self._pool = SubscriberPool(self.context, self._receive)
        self._pool.start_poll_thread()
        self.request(CHIRPServiceIdentifier.MONITORING)

    def set_topics(self, new_topics: list[str]):
        """Set topics to subscribe to"""
        with self._topics_lock:
            # Unsubscribe from old topics
            old_topics = self._topics
            for topic in old_topics:
                if topic not in new_topics:
                    self._pool.unsubscribe(topic)
            # Subscribe to new topics
            for topic in new_topics:
                if topic not in old_topics:
                    self._pool.subscribe(topic)
            self._topics = new_topics

    def receive_log(self, record: logging.LogRecord) -> None:
        """Callback for receiving a log message"""
        pass

    def receive_metric(self, sender: str, metric: Metric, timestamp: datetime, value: Any) -> None:
        """Callback for receiving a metric"""
        pass

    def receive_notification(self, sender: str, topics_prefix: str, topics: dict[str, str]) -> None:
        """Callback for receiving a notification"""
        pass

    def _pool_checker(self) -> None:
        while self._com_thread_evt and not self._com_thread_evt.is_set():
            self._pool.check_exception()
            time.sleep(0.1)

    def _add_com_thread(self) -> None:
        super()._add_com_thread()
        self._com_thread_pool["monitoring_listener"] = Thread(target=self._pool_checker, daemon=True)
        self.log_cmdp_l.debug("Monitoring listener thread prepared and added to the pool.")

    def _receive(self, frames: list[bytes]) -> None:
        try:
            msg = CMDP1Message.disassemble(frames)
            if msg.is_log_message():
                log_msg = CMDP1LogMessage.from_cmdp_message(msg)
                self.log_cmdp_l.trace(
                    "Received log message from %s with level %s and topic %s",
                    log_msg.sender,
                    log_msg.log_level.name,
                    log_msg.log_topic,
                )
                self.receive_log(log_msg.to_log_record())
            elif msg.is_stat_message():
                stat_msg = CMDP1StatMessage.from_cmdp_message(msg)
                self.log_cmdp_l.trace(
                    "Received metric from %s with topic %s and value %s%s",
                    stat_msg.sender,
                    stat_msg.metric.name,
                    stat_msg.value,
                    stat_msg.metric.unit,
                )
                self.receive_metric(stat_msg.sender, stat_msg.metric, stat_msg.time, stat_msg.value)
            elif msg.is_notification():
                notification = CMDP1Notification.from_cmdp_message(msg)
                self.log_cmdp_l.trace(
                    "Received notification from %s with topics prefix %s", notification.sender, notification.topics_prefix
                )
                self.receive_notification(notification.sender, notification.topics_prefix, notification.topics)
        except MessageDecodingError as e:
            self.log_cmdp_l.warning("Failed to decode CMDP message: %s", str(e))

    @chirp_callback(CHIRPServiceIdentifier.MONITORING)
    def _cmdp_chirp_callback(self, service: DiscoveredService) -> None:
        if service.alive:
            with self._topics_lock:
                self._pool.add_socket(service.host_uuid, service.address, service.port)
                for topic in self._topics:
                    self._pool.subscribe(topic, service.host_uuid)
        else:
            self._pool.remove_socket(service.host_uuid)

    def reentry(self) -> None:
        self._pool.stop_poll_thread()
        super().reentry()


class StandaloneListener(MonitoringListener):
    """Class to run a listener outside of a satellite or controller"""

    def __init__(self, name: str, group: str, interface: list[str] | None, **kwds: Any):
        super().__init__(name=name, group=group, interface=interface, **kwds)

        # Set up background communication threads
        super()._add_com_thread()
        super()._start_com_threads()

    def run_listener(self) -> None:
        while self._com_thread_evt and not self._com_thread_evt.is_set():
            try:
                try:
                    # blocking call but with timeout to prevent deadlocks
                    task = self.task_queue.get(block=True, timeout=0.1)
                    callback = task[0]
                    args = task[1]
                    try:
                        callback(*args)
                    except Exception as e:
                        self.log.exception(
                            "Caught exception handling task '%s' with args '%s': %s",
                            callback,
                            args,
                            repr(e),
                        )
                except Empty:
                    # nothing to process
                    time.sleep(0.1)
            except KeyboardInterrupt:
                self.log.warning("Caught KeyboardInterrupt, shutting down.")
                break

        self.terminate()
