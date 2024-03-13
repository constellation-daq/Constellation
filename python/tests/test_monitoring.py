"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0
"""

import pytest
import logging

from constellation.cmdp import CMDPTransmitter, Metric, MetricsType

# from constellation.monitoring import ZeroMQSocketListener, MonitoringManager

from conftest import mock_packet_queue_sender, mocket, send_port


@pytest.fixture
def mock_transmitter_a():
    """Mock Transmitter endpoint A."""
    m = mocket()
    m.port = send_port
    cmdp = CMDPTransmitter("mock_cmdp", m)
    yield cmdp, m


@pytest.fixture
def mock_transmitter_b(mock_socket_sender):
    """Mock Transmitter endpoint B."""
    cmdp = CMDPTransmitter("mock_cmdp", mock_socket_sender)
    yield cmdp


def test_log_transmission(mock_transmitter_a, mock_transmitter_b):
    cmdp, m = mock_transmitter_a
    log = logging.getLogger()
    rec1 = log.makeRecord("name", 10, __name__, 42, "mock log message", None, None)
    cmdp.send_log(rec1)
    # check that we have a packet ready to be read
    assert len(mock_packet_queue_sender[send_port]) == 3
    rec2 = mock_transmitter_b.recv()
    # check that the packet is processed
    assert len(mock_packet_queue_sender[send_port]) == 0
    assert rec2.getMessage() == "mock log message"


def test_stat_transmission(mock_transmitter_a, mock_transmitter_b):
    cmdp, m = mock_transmitter_a
    m1 = Metric("mock_val", "a mocked value", "Mmocs", MetricsType.LAST_VALUE, 42)
    assert not m1.sender
    cmdp.send(m1)
    # check that we have a packet ready to be read
    assert len(mock_packet_queue_sender[send_port]) == 3
    m2 = mock_transmitter_b.recv()
    # check that the packet is processed
    assert len(mock_packet_queue_sender[send_port]) == 0
    assert m2.name == "mock_val"
    assert m2.value == 42
    assert m2.sender == "mock_cmdp"
    assert m2.time
