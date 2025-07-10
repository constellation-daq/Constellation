"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2
"""

import logging
import os
import time
from unittest.mock import MagicMock

import pytest
import zmq
from conftest import DEFAULT_SEND_PORT

from constellation.core.chirp import CHIRPBeaconTransmitter, CHIRPMessageType, CHIRPServiceIdentifier
from constellation.core.cmdp import CMDPPublisher, CMDPTransmitter, Metric, MetricsType, Notification
from constellation.core.monitoring import MonitoringSender, ZeroMQSocketLogListener, schedule_metric


@pytest.fixture
def mock_transmitter_a(mock_socket_receiver):
    """Mock Transmitter endpoint A."""
    cmdp = CMDPTransmitter("mock_cmdp", mock_socket_receiver)
    yield cmdp, mock_socket_receiver


@pytest.fixture
def mock_transmitter_b(mock_socket_sender):
    """Mock Transmitter endpoint B."""
    cmdp = CMDPTransmitter("mock_cmdp", mock_socket_sender)
    yield cmdp, mock_socket_sender


@pytest.fixture
def mock_monitoringsender(mock_zmq_context):
    """Create a mock MonitoringSender instance."""
    ctx = mock_zmq_context()

    class MyStatProducer(MonitoringSender):
        @schedule_metric("Answer", MetricsType.LAST_VALUE, 0.1)
        def get_answer(self):
            """The answer to the Ultimate Question"""
            # self.log.info("Got the answer!")
            return 42

    m = MyStatProducer(name="mock_sender", interface="*", mon_port=DEFAULT_SEND_PORT)
    m._add_com_thread()
    m._start_com_threads()
    time.sleep(0.1)
    yield m, ctx
    # teardown
    m.reentry()


@pytest.fixture
def monitoringsender():
    """Create a MonitoringSender instance."""

    class MyStatProducer(MonitoringSender):
        @schedule_metric("Answer", MetricsType.LAST_VALUE, 0.5)
        def get_answer(self):
            """The answer to the Ultimate Question"""
            # self.log.info("Got the answer!")
            return 42

    m = MyStatProducer(name="mock_sender", interface="*", mon_port=DEFAULT_SEND_PORT)
    yield m
    m.reentry()


@pytest.fixture
def mock_listener(mock_transmitter_a):
    """Create a mock log listener instance."""

    mock_handler = MagicMock()
    mock_handler.handle.return_value = None
    mock_handler.emit.return_value = None
    mock_handler.create_lock.return_value = None
    mock_handler.lock = None
    tm, sock = mock_transmitter_a
    listener = ZeroMQSocketLogListener(tm, mock_handler)
    yield listener, mock_handler, sock


def test_log_transmission(mock_transmitter_a, mock_transmitter_b):
    cmdp, m = mock_transmitter_a
    log = logging.getLogger()
    rec1 = log.makeRecord("name", 10, __name__, 42, "mock log message", None, None)
    cmdp.send_log(rec1)
    # check that we have a packet ready to be read
    assert len(m.packet_queue_out[DEFAULT_SEND_PORT]) == 3
    tm, _sock = mock_transmitter_b
    rec2 = tm.recv()
    # check that the packet is processed
    assert len(m.packet_queue_out[DEFAULT_SEND_PORT]) == 0
    assert rec2.getMessage() == "mock log message"


def test_stat_transmission(mock_transmitter_a, mock_transmitter_b):
    cmdp, m = mock_transmitter_a
    m1 = Metric("mock_val", "Mmocs", MetricsType.LAST_VALUE, 42)
    assert not m1.sender
    cmdp.send(m1)
    # check that we have a packet ready to be read
    assert len(m.packet_queue_out[DEFAULT_SEND_PORT]) == 3
    tm, _sock = mock_transmitter_b
    m2 = tm.recv()
    # check that the packet is processed
    assert len(m.packet_queue_out[DEFAULT_SEND_PORT]) == 0
    assert m2.name == "MOCK_VAL"
    assert m2.value == 42
    assert m2.sender == "mock_cmdp"
    assert m2.time


def test_log_monitoring(mock_listener, mock_monitoringsender):
    """Test that log messages are received, decoded and enqueued by ZeroMQSocketLogListener."""
    listener, stream, sock = mock_listener
    ms, ctx = mock_monitoringsender
    assert ms._zmq_log_handler, "ZMQ CMDP log handler not properly set up"
    sock.setsockopt_string(zmq.SUBSCRIBE, "LOG/")
    lr = ms.get_logger("mock_sender")
    assert ms._zmq_log_handler in lr.handlers, "ZMQ handler not present"
    # allow subscription to be picked up
    time.sleep(0.2)
    lr.warning("mock warning before start")
    time.sleep(0.1)
    assert len(ctx.packet_queue_out[DEFAULT_SEND_PORT]) == 3
    listener.start()
    time.sleep(0.1)
    # processed?
    assert len(ctx.packet_queue_out[DEFAULT_SEND_PORT]) == 0
    assert stream.handle.called
    # check arg to mock call
    assert isinstance(stream.mock_calls[0][1][0], logging.LogRecord)
    lr.info("mock info")
    time.sleep(0.1)
    assert len(ctx.packet_queue_out[DEFAULT_SEND_PORT]) == 0
    assert len(stream.mock_calls) == 2


def test_log_levels(mock_listener, mock_monitoringsender):
    ms, ctx = mock_monitoringsender
    listener, stream, sock = mock_listener
    sock.setsockopt_string(zmq.SUBSCRIBE, "LOG/")
    time.sleep(0.2)
    lr = ms.get_logger("mock_sender")
    # log a custom level
    lr.trace("mock trace message")
    lr.status("mock status message")
    time.sleep(0.1)
    ctx.packet_queue_out[DEFAULT_SEND_PORT][0].startswith(b"LOG/TRACE")
    ctx.packet_queue_out[DEFAULT_SEND_PORT][3].startswith(b"LOG/STATUS")
    # error mapped to critical?
    lr.error("mock critical!!!!")
    time.sleep(0.1)
    ctx.packet_queue_out[DEFAULT_SEND_PORT][3].startswith(b"LOG/CRITICAL")
    # check reconstruction
    listener.start()
    time.sleep(0.1)
    assert isinstance(stream.mock_calls[0][1][0], logging.LogRecord)
    assert isinstance(stream.mock_calls[1][1][0], logging.LogRecord)
    assert isinstance(stream.mock_calls[2][1][0], logging.LogRecord)
    assert stream.mock_calls[0][1][0].levelname == "TRACE"
    assert stream.mock_calls[1][1][0].levelname == "STATUS"
    assert stream.mock_calls[2][1][0].levelname == "CRITICAL"


def test_log_exception(mock_listener, mock_monitoringsender):
    """Test use of logging.exception call."""
    listener, stream, sock = mock_listener
    sock.setsockopt_string(zmq.SUBSCRIBE, "LOG/")
    time.sleep(0.2)
    ms, ctx = mock_monitoringsender
    lr = ms.get_logger("mock_sender")
    # log an exception
    try:
        raise RuntimeError("mock an exception")
    except RuntimeError:
        lr.exception("caught mock exception")
    time.sleep(0.1)
    ctx.packet_queue_out[DEFAULT_SEND_PORT][0].startswith(b"LOG/CRITICAL")
    # check reconstruction
    listener.start()
    time.sleep(0.1)
    assert isinstance(stream.mock_calls[0][1][0], logging.LogRecord)
    assert stream.mock_calls[0][1][0].levelname == "CRITICAL"


def test_monitoring_sender_init(mock_listener, mock_monitoringsender):
    m, _ctx = mock_monitoringsender
    # is our method registered?
    assert len(m._metrics_callbacks) == 1


def test_monitoring_sender_loop(mock_listener, mock_monitoringsender):
    """Test that the MonitoringSender loop works as expected."""
    m, ctx = mock_monitoringsender
    # not subscribed yet, expect nothing in the mock network data stream
    assert not ctx.packet_queue_out[DEFAULT_SEND_PORT]
    listener, stream, sock = mock_listener
    # subscribe
    sock.setsockopt_string(zmq.SUBSCRIBE, "STAT/")
    time.sleep(0.2)
    assert b"STAT/GET_ANSWER" in ctx.packet_queue_out[DEFAULT_SEND_PORT]


def test_monitoring_notification(mock_transmitter_a, mock_monitoringsender):
    """Test that the MonitoringSender loop works as expected."""
    m, ctx = mock_monitoringsender
    # not subscribed yet, expect nothing in the mock network data stream
    assert not ctx.packet_queue_out[DEFAULT_SEND_PORT]
    cmdptrans, sock = mock_transmitter_a
    # subscribe
    sock.setsockopt_string(zmq.SUBSCRIBE, "STAT?")
    time.sleep(0.2)
    msg = cmdptrans.recv()
    assert msg
    assert isinstance(msg, Notification)
    assert "GET_ANSWER" in msg.topics.keys()
    assert "Ultimate Question" in msg.topics["GET_ANSWER"]


@pytest.mark.constellation("monitoring_file_writing")
def test_monitoring_file_writing(monitoringlistener, monitoringsender):
    ml, tmpdir = monitoringlistener
    ms = monitoringsender
    assert len(ml._log_listeners) == 0
    chirp = CHIRPBeaconTransmitter("mock_sender", "monitoring_file_writing", interface_addresses=["127.0.0.1"])
    chirp.emit(CHIRPServiceIdentifier.MONITORING, CHIRPMessageType.OFFER, DEFAULT_SEND_PORT)
    # start metric sender thread
    ms._add_com_thread()
    ms._start_com_threads()
    time.sleep(1)
    assert len(ml._log_listeners) == 1
    assert len(ml._metric_sockets) == 1
    assert os.path.exists(os.path.join(tmpdir, "logs")), "Log output directory not created"
    assert os.path.exists(os.path.join(tmpdir, "stats")), "Stats output directory not created"
    assert os.path.exists(os.path.join(tmpdir, "logs", "monitoring_file_writing.log")), "No log file created"
    assert os.path.exists(
        os.path.join(tmpdir, "stats", "MyStatProducer.mock_sender.get_answer.csv")
    ), "Expected output metrics csv not found"
    # teardown
    chirp.close()


def test_monitoring_subscriptions():
    """Test the subscription to log messages."""
    context = zmq.Context()
    cmdp_socket = context.socket(zmq.XPUB)
    port = cmdp_socket.bind_to_random_port("tcp://*")
    pub = CMDPPublisher("subscription_test", cmdp_socket)
    assert not pub.subscriptions
    # create socket for subscription
    socket = context.socket(zmq.SUB)
    socket.connect(f"tcp://127.0.0.1:{port}")
    # add timeout to avoid deadlocks
    socket.setsockopt(zmq.RCVTIMEO, 250)
    # create test log records
    log = logging.getLogger()
    # make log message that will be subscribed to
    rec = log.makeRecord("TESTLOGS", logging.getLevelName("CRITICAL"), __name__, 42, "selected", None, None)
    # make log message that will only be shown if a log level is subscribed to
    rec_lvl = log.makeRecord("DIFFERENTLOGS", logging.getLevelName("CRITICAL"), __name__, 42, "only if level", None, None)
    # make a log message that will only be shown if all logs are subscribed to
    rec_glb = log.makeRecord("DIFFERENTLOGS", logging.getLevelName("DEBUG"), __name__, 42, "only if global", None, None)
    topics = ["LOG/", "LOG/CRITICAL", "LOG/CRITICAL/TESTLOGS"]
    for topic in topics:
        # subscribe to log topic
        print(f"Subscribing to {topic}")
        socket.setsockopt_string(zmq.SUBSCRIBE, topic)
        time.sleep(0.1)
        pub.update_subscriptions()
        assert topic in pub.subscriptions.keys(), "Subscription unsuccessful"
        assert pub.subscriptions[topic] == 1, "Subscription unsuccessful"
        assert pub.has_log_subscribers(rec), "Subscription unsuccessful"
        if len(topic.removesuffix("/").split("/")) < 3:
            assert pub.has_log_subscribers(rec_lvl), "Failed to identify record with same level as subscription"
        else:
            assert not pub.has_log_subscribers(rec_lvl), "Failed to suppress log message on same level"
        if len(topic.removesuffix("/").split("/")) == 1:
            assert pub.has_log_subscribers(rec_glb), "Failed to pass record while subscribing to all logs"
        else:
            assert not pub.has_log_subscribers(rec_glb), "Failed to suppress unrelated log message."
        # cancel subscription
        socket.setsockopt_string(zmq.UNSUBSCRIBE, topic)
        time.sleep(0.1)
        pub.update_subscriptions()
        assert topic not in pub.subscriptions.keys(), "Unsubscription unsuccessful"
        assert not pub.subscriptions
        assert not pub.has_log_subscribers(rec), "Unsubscription unsuccessful"
    # teardown
    cmdp_socket.close()
    socket.close()
    context.term()
