"""
SPDX-FileCopyrightText: 2026 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2
"""

import logging
import time
from datetime import datetime
from typing import Any

import pytest

from constellation.core.chirp import get_uuid
from constellation.core.listener import MonitoringListener
from constellation.core.monitoring import MonitoringSender, schedule_metric
from constellation.core.network import get_loopback_interface_name
from constellation.core.protocol.cmdp1 import LogLevel, Metric

from .conftest import DEFAULT_SEND_PORT


class MyMonitoringSender(MonitoringSender):
    def __init__(self, *args: Any, **kwargs: Any):
        super().__init__(*args, **kwargs)
        self.reset_metrics()
        self.register_metric("TEST", "", "Some random test value.")

    @schedule_metric("", 0.01)
    def answer(self) -> int:
        """The answer to everything."""
        return 42

    def emit_stat(self, value: Any) -> None:
        self.stat("TEST", value)

    def emit_log(self, msg: str) -> None:
        self.log.status(msg)


class MyMonitoringListener(MonitoringListener):
    def __init__(self, *args: Any, **kwargs: Any):
        super().__init__(*args, **kwargs)
        self._received_log_records: list[logging.LogRecord] = []
        self._received_metrics: list[tuple[str, Metric, datetime, Any]] = []
        self._received_notifications: list[tuple[str, str, dict[str, str]]] = []

    def receive_log(self, record: logging.LogRecord) -> None:
        super().receive_log(record)  # for coverage
        self._received_log_records.append(record)

    def receive_metric(self, sender: str, metric: Metric, timestamp: datetime, value: Any) -> None:
        super().receive_metric(sender, metric, timestamp, value)  # for coverage
        self._received_metrics.append((sender, metric, timestamp, value))

    def receive_notification(self, sender: str, topics_prefix: str, topics: dict[str, str]) -> None:
        super().receive_notification(sender, topics_prefix, topics)  # for coverage
        self._received_notifications.append((sender, topics_prefix, topics))


@pytest.fixture
def monitoringsender():
    """Create a MonitoringSender instance."""

    m = MyMonitoringSender(name="sender", interface=[get_loopback_interface_name()], mon_port=DEFAULT_SEND_PORT)

    # since setup_logging was not called, add cmdp log handler manually
    logging.root.addHandler(m._zmq_log_handler)

    m._add_com_thread()
    m._start_com_threads()

    yield m

    # teardown
    logging.root.removeHandler(m._zmq_log_handler)
    m.reentry()


@pytest.fixture
def monitoringlistener():
    """Create MonitoringListener instance."""

    m = MyMonitoringListener(name="listener", group="mockstellation", interface=[get_loopback_interface_name()])

    m._add_com_thread()
    m._start_com_threads()

    m._pool.add_socket(get_uuid("MyMonitoringSender"), "127.0.0.1", DEFAULT_SEND_PORT)

    yield m

    m.set_topics([])
    m.reentry()


def test_log(monitoringsender: MyMonitoringSender, monitoringlistener: MyMonitoringListener):
    # Subscribe to status logs
    monitoringlistener.set_topics(["LOG/STATUS"])

    # Wait for subscription
    timeout = 2.0
    while timeout > 0.0 and not monitoringsender.should_log(LogLevel.STATUS):
        timeout -= 0.01
        time.sleep(0.01)
    assert timeout > 0.0, "Subscription not received"

    # Emit a status log
    monitoringsender.emit_log("Test log")

    # Wait for log message
    timeout = 2.0
    while timeout > 0.0 and not monitoringlistener._received_log_records:
        timeout -= 0.01
        time.sleep(0.01)
    assert timeout > 0.0, "Message not received"

    # Check log message
    assert len(monitoringlistener._received_log_records) == 1
    record = monitoringlistener._received_log_records[0]
    assert record.levelname == "STATUS"
    assert record.name == "MyMonitoringSender".upper()
    assert record.getMessage() == "Test log"


def test_stat(monitoringsender: MyMonitoringSender, monitoringlistener: MyMonitoringListener):
    # Subscribe to test metric
    monitoringlistener.set_topics(["STAT/TEST"])

    # Wait for subscription
    timeout = 2.0
    while timeout > 0.0 and not monitoringsender.should_stat("TEST"):
        timeout -= 0.01
        time.sleep(0.01)
    assert timeout > 0.0, "Subscription not received"

    # Emit test metric
    monitoringsender.emit_stat(3.14)

    # Wait for stat message
    timeout = 2.0
    while timeout > 0.0 and not monitoringlistener._received_metrics:
        timeout -= 0.01
        time.sleep(0.01)
    assert timeout > 0.0, "Message not received"

    # Check stat message
    assert len(monitoringlistener._received_metrics) == 1
    sender, metric, metric_time, value = monitoringlistener._received_metrics[0]
    assert sender == "MyMonitoringSender.sender"
    assert metric.name == "TEST"
    assert value == 3.14


def test_scheduled_metric(monitoringsender: MyMonitoringSender, monitoringlistener: MyMonitoringListener):
    # Reset metrics for coverage
    monitoringsender.reset_metrics()

    # Subscribe to test metric
    monitoringlistener.set_topics(["STAT/ANSWER"])

    # Wait for subscription
    timeout = 2.0
    while timeout > 0.0 and not monitoringsender.should_stat("ANSWER"):
        timeout -= 0.01
        time.sleep(0.01)
    assert timeout > 0.0, "Subscription not received"

    # Wait for stat message
    timeout = 2.0
    while timeout > 0.0 and not monitoringlistener._received_metrics:
        timeout -= 0.01
        time.sleep(0.01)
    assert timeout > 0.0, "Message not received"

    # Check stat message
    assert len(monitoringlistener._received_metrics) >= 1
    sender, metric, metric_time, value = monitoringlistener._received_metrics[0]
    assert sender == "MyMonitoringSender.sender"
    assert metric.name == "ANSWER"
    assert value == 42


def test_register_scheduled_metric(monitoringsender: MyMonitoringSender, monitoringlistener: MyMonitoringListener):
    # Subscribe to test metric
    monitoringlistener.set_topics(["STAT/SCHED"])

    # Wait for subscription
    timeout = 2.0
    while timeout > 0.0 and not monitoringsender.should_stat("SCHED"):
        timeout -= 0.01
        time.sleep(0.01)
    assert timeout > 0.0, "Subscription not received"

    # Register new scheduled metric
    monitoringsender.register_scheduled_metric("SCHED", "pings", "", 0.01, lambda: 1)

    # Wait for stat message
    timeout = 2.0
    while timeout > 0.0 and not monitoringlistener._received_metrics:
        timeout -= 0.01
        time.sleep(0.01)
    assert timeout > 0.0, "Message not received"

    # Check stat message
    assert len(monitoringlistener._received_metrics) >= 1
    sender, metric, metric_time, value = monitoringlistener._received_metrics[0]
    assert sender == "MyMonitoringSender.sender"
    assert metric.name == "SCHED"
    assert metric.unit == "pings"
    assert value == 1


def test_nonreturning_scheduled_metric(monitoringsender: MyMonitoringSender, monitoringlistener: MyMonitoringListener):
    class TestCls:
        def __init__(self):
            self.none_counts = 0
            self.throw = False

        def cb(self) -> None:
            if self.throw:
                raise Exception("throwing as requested")
            self.none_counts += 1
            return None

    testcls = TestCls()

    # Subscribe to test metric
    monitoringlistener.set_topics(["STAT/SCHED", "LOG/CRITICAL/MNTR"])

    # Wait for subscription
    timeout = 2.0
    while (
        timeout > 0.0
        and not monitoringsender.should_stat("SCHED")
        and not monitoringsender.should_log(LogLevel.CRITICAL, "MNTR")
    ):
        timeout -= 0.01
        time.sleep(0.01)
    assert timeout > 0.0, "Subscription not received"

    # Register new scheduled metric
    monitoringsender.register_scheduled_metric("SCHED", "pings", "", 0.01, lambda: testcls.cb())

    # Wait for first iteration to check returning None
    timeout = 2.0
    while timeout > 0.0 and testcls.none_counts == 0:
        timeout -= 0.01
        time.sleep(0.01)
    assert timeout > 0.0, "Scheduled metric never called"

    # Throw in callback
    testcls.throw = True

    # Wait for log message
    timeout = 2.0
    while timeout > 0.0 and not monitoringlistener._received_log_records:
        timeout -= 0.01
        time.sleep(0.01)
    assert timeout > 0.0, "Message not received"

    # Check log message
    assert len(monitoringlistener._received_log_records) >= 1
    record = monitoringlistener._received_log_records[0]
    assert record.levelname == "CRITICAL"
    assert record.name == "MNTR"
    assert record.getMessage() == "Could not get metric SCHED: Exception('throwing as requested')"


def test_emit_unregsitered_metric(monitoringsender: MyMonitoringSender, monitoringlistener: MyMonitoringListener):
    # Unregister test metric
    monitoringsender.unregister_metric("TEST")

    # Subscribe to warning logs
    monitoringlistener.set_topics(["LOG/WARNING/MNTR"])

    # Wait for subscription
    timeout = 2.0
    while timeout > 0.0 and not monitoringsender.should_log(LogLevel.WARNING, "MNTR"):
        timeout -= 0.01
        time.sleep(0.01)
    assert timeout > 0.0, "Subscription not received"

    # Emit warning by trying to send unregistered metric
    monitoringsender.emit_stat(3.14)

    # Wait for log message
    timeout = 2.0
    while timeout > 0.0 and not monitoringlistener._received_log_records:
        timeout -= 0.01
        time.sleep(0.01)
    assert timeout > 0.0, "Message not received"

    # Check log message
    assert len(monitoringlistener._received_log_records) == 1
    record = monitoringlistener._received_log_records[0]
    assert record.levelname == "WARNING"
    assert record.name == "MNTR"
    assert record.getMessage() == "Cannot stat TEST: metric not registered"


def test_notifications(monitoringsender: MyMonitoringSender, monitoringlistener: MyMonitoringListener):
    # Subscribe to notifications
    monitoringlistener.set_topics(["LOG?", "STAT?"])

    # Wait for notification
    timeout = 2.0
    while timeout > 0.0 and not len(monitoringlistener._received_notifications) >= 2:
        timeout -= 0.01
        time.sleep(0.01)
    assert timeout > 0.0, "Message not received"

    # Merge all notifications
    merged_topics: dict[str, str] = {}
    for _, topics_prefix, topics in monitoringlistener._received_notifications:
        for topic, description in topics.items():
            merged_topics[f"{topics_prefix}/{topic}"] = description

    assert "STAT/ANSWER" in merged_topics
    assert merged_topics["STAT/ANSWER"] == "The answer to everything."
    assert "STAT/TEST" in merged_topics
    assert merged_topics["STAT/TEST"] == "Some random test value."
    assert f"LOG/{'MyMonitoringSender'.upper()}" in merged_topics
    assert "LOG/MNTR" in merged_topics


def test_notifications_new_topic(monitoringsender: MyMonitoringSender, monitoringlistener: MyMonitoringListener):
    # Subscribe to notifications
    monitoringlistener.set_topics(["LOG?", "STAT?"])

    # Wait for notification
    timeout = 2.0
    while timeout > 0.0 and not len(monitoringlistener._received_notifications) >= 2:
        timeout -= 0.01
        time.sleep(0.01)
    assert timeout > 0.0, "Message not received"

    # Delete stored notifications
    monitoringlistener._received_notifications.clear()

    # Register a new log topic
    monitoringsender.get_logger("new_log_topic")

    # Wait for notification
    timeout = 2.0
    while timeout > 0.0 and not monitoringlistener._received_notifications:
        timeout -= 0.01
        time.sleep(0.01)
    assert timeout > 0.0, "Message not received"

    # Check new log topic present in topics
    assert len(monitoringlistener._received_notifications)
    _, topics_prefix, topics = monitoringlistener._received_notifications[0]
    assert topics_prefix == "LOG"
    assert "NEW_LOG_TOPIC" in topics

    # Delete stored notifications
    monitoringlistener._received_notifications.clear()

    # Register a new stat topic
    monitoringsender.register_metric("new_stat_topic", "", "Some new metric.")

    # Wait for notification
    timeout = 2.0
    while timeout > 0.0 and not monitoringlistener._received_notifications:
        timeout -= 0.01
        time.sleep(0.01)
    assert timeout > 0.0, "Message not received"

    # Check new stat topic present in topics
    assert len(monitoringlistener._received_notifications)
    _, topics_prefix, topics = monitoringlistener._received_notifications[0]
    assert topics_prefix == "STAT"
    assert "NEW_STAT_TOPIC" in topics
    assert topics["NEW_STAT_TOPIC"] == "Some new metric."

    # Delete stored notifications
    monitoringlistener._received_notifications.clear()

    # Unregister stat topic
    monitoringsender.unregister_metric("new_STAT_topic")

    # Wait for notification
    timeout = 2.0
    while timeout > 0.0 and not monitoringlistener._received_notifications:
        timeout -= 0.01
        time.sleep(0.01)
    assert timeout > 0.0, "Message not received"

    # Check stat topic got removed from topics
    assert len(monitoringlistener._received_notifications)
    _, topics_prefix, topics = monitoringlistener._received_notifications[0]
    assert topics_prefix == "STAT"
    assert "NEW_STAT_TOPIC" not in topics
