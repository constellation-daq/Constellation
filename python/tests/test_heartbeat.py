"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2
"""

import time
from unittest.mock import MagicMock, patch

import pytest
from conftest import mock_packet_queue_recv, mock_packet_queue_sender, mocket

from constellation.core.chirp import get_uuid
from constellation.core.heartbeater import HeartbeatSender
from constellation.core.message.cscp1 import SatelliteState
from constellation.core.network import get_loopback_interface_name

HB_PORT = 33333


@pytest.fixture
def mock_heartbeat_sender():
    """Create a mock HeartbeatSender base instance."""

    def mocket_factory(*args, **kwargs):
        m = mocket()
        m.endpoint = 0
        return m

    with patch("constellation.core.base.zmq.Context") as mock:
        mock_context = MagicMock()
        mock_context.socket = mocket_factory
        mock.return_value = mock_context
        hbs = HeartbeatSender(
            name="mock_heartbeater",
            group="mockstellation",
            hb_port=HB_PORT,
            interface=[get_loopback_interface_name()],
        )
        hbs._add_com_thread()
        hbs._start_com_threads()
        # give the threads a chance to start
        time.sleep(0.1)
        yield hbs


@pytest.mark.forked
def test_hb_check_init(mock_heartbeat_checker):
    hbc = mock_heartbeat_checker
    assert not hbc.get_failed()


@pytest.mark.forked
def test_hb_check_register(mock_heartbeat_checker):
    hbc = mock_heartbeat_checker
    hbc.register_heartbeat_host(get_uuid("mock_sender"), "tcp://127.0.0.1:44444")
    time.sleep(0.5)
    assert not hbc.get_failed()


@pytest.mark.forked
def test_hb_send_recv(mock_heartbeat_sender, mock_heartbeat_checker):
    hbc = mock_heartbeat_checker
    hbs = mock_heartbeat_sender
    hbc.HB_INIT_PERIOD = 180
    hbs.default_period = 200
    hbc.register_heartbeat_host(get_uuid("HeartbeatSender.mock_heartbeater"), f"tcp://127.0.0.1:{HB_PORT}")
    time.sleep(0.5)
    assert not hbc.get_failed()
    hbs.fsm.initialize("running mock init")
    hbs.fsm.initialized("done with mock init")
    time.sleep(0.5)
    assert hbc.heartbeat_states["HeartbeatSender.mock_heartbeater"] == SatelliteState.INIT


@pytest.mark.forked
def test_hb_send_recv_lag(mock_heartbeat_sender, mock_heartbeat_checker):
    """Test that receiver can catch up to sender."""
    hbs = mock_heartbeat_sender
    hbs.default_period = 120
    hbc = mock_heartbeat_checker
    hbc.HB_INIT_PERIOD = 180
    time.sleep(2)
    stack = len(mock_packet_queue_sender[HB_PORT])
    assert stack >= 8
    hbc.register_heartbeat_host(get_uuid("HeartbeatSender.mock_heartbeater"), f"tcp://127.0.0.1:{HB_PORT}")
    hbs.fsm.initialize("running mock init")
    hbs.fsm.initialized("done with mock init")
    time.sleep(1)
    assert len(mock_packet_queue_sender[HB_PORT]) <= stack / 2
    assert hbc.heartbeat_states["HeartbeatSender.mock_heartbeater"] == SatelliteState.INIT
    assert not hbc.get_failed()


@pytest.mark.forked
def test_hb_extrasystoles(mock_heartbeat_sender):
    """Test that sender can send extrasystoles."""
    hbs = mock_heartbeat_sender
    hbs.default_period = 20000
    assert HB_PORT not in mock_packet_queue_recv
    hbs.fsm.initialize("")
    time.sleep(0.3)
    hbs.fsm.initialized("")
    time.sleep(0.3)
    assert HB_PORT in mock_packet_queue_sender
    assert len(mock_packet_queue_sender[HB_PORT]) == 2


@pytest.mark.forked
def test_hb_recv_fail(mock_heartbeat_checker):
    hbc = mock_heartbeat_checker
    hbc.HB_INIT_PERIOD = 200
    hbc.register_heartbeat_host(get_uuid("HeartbeatSender.mock_heartbeater"), "tcp://127.0.0.1:23456")
    timeout = 4
    while timeout > 0 and not hbc.get_failed():
        time.sleep(0.1)
        timeout -= 0.1
    assert hbc.get_failed()
