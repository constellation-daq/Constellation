"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2
"""

import time

import pytest

from constellation.core.chirp import get_uuid
from constellation.core.heartbeater import HeartbeatSender
from constellation.core.message.cscp1 import SatelliteState
from constellation.core.network import get_loopback_interface_name

HB_PORT = 33333


@pytest.fixture
def mock_heartbeat_sender(mock_zmq_context):
    """Create a mock HeartbeatSender base instance."""
    ctx = mock_zmq_context()
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
    yield hbs, ctx
    # teardown
    hbs.reentry()


def test_hb_check_init(mock_heartbeat_checker):
    hbc, _ctx = mock_heartbeat_checker
    assert not hbc.get_failed()


def test_hb_check_register(mock_heartbeat_checker):
    hbc, _ctx = mock_heartbeat_checker
    hbc.register_heartbeat_host(get_uuid("mock_sender"), "tcp://127.0.0.1:44444")
    time.sleep(0.5)
    assert not hbc.get_failed()


def test_hb_send_recv_comm(mock_heartbeat_sender, mock_heartbeat_checker):
    hbc, ctx1 = mock_heartbeat_checker
    hbs, ctx2 = mock_heartbeat_sender
    hbc.HB_INIT_PERIOD = 180
    hbs.default_heartbeat_period = 200
    hbc.register_heartbeat_host(get_uuid("HeartbeatSender.mock_heartbeater"), f"tcp://127.0.0.1:{HB_PORT}")
    time.sleep(0.5)
    assert not hbc.get_failed()
    hbs.fsm.initialize("running mock init")
    hbs.fsm.initialized("done with mock init")
    time.sleep(0.5)
    assert hbc.heartbeat_states["HeartbeatSender.mock_heartbeater"] == SatelliteState.INIT


def test_hb_send_recv_lag(mock_heartbeat_sender, mock_heartbeat_checker):
    """Test that receiver can catch up to sender."""
    hbs, ctx_s = mock_heartbeat_sender
    hbs.default_heartbeat_period = 120
    hbc, ctx_c = mock_heartbeat_checker
    hbs.default_period = 120
    hbc.HB_INIT_PERIOD = 180
    time.sleep(2)
    timeout = 2
    stack = len(ctx_s.packet_queue_out[HB_PORT])
    while timeout > 0 and stack < 8:
        timeout -= 0.01
        time.sleep(0.01)
        stack = len(ctx_s.packet_queue_out[HB_PORT])
    assert stack >= 8
    hbc.register_heartbeat_host(get_uuid("HeartbeatSender.mock_heartbeater"), f"tcp://127.0.0.1:{HB_PORT}")
    hbs.fsm.initialize("running mock init")
    hbs.fsm.initialized("done with mock init")
    time.sleep(1)
    newstack = len(ctx_s.packet_queue_out[HB_PORT])
    while timeout > 0 and newstack > stack / 2:
        timeout -= 0.01
        time.sleep(0.01)
        newstack = len(ctx_s.packet_queue_out[HB_PORT])
    assert newstack <= stack / 2
    assert hbc.heartbeat_states["HeartbeatSender.mock_heartbeater"] == SatelliteState.INIT
    assert not hbc.get_failed()


def test_hb_extrasystoles(mock_heartbeat_sender):
    """Test that sender can send extrasystoles."""
    hbs, ctx = mock_heartbeat_sender
    hbs.default_heartbeat_period = 20000
    assert not ctx.packet_queue_out[HB_PORT]
    hbs.fsm.initialize("")
    time.sleep(0.3)
    hbs.fsm.initialized("")
    time.sleep(0.3)
    assert HB_PORT in ctx.packet_queue_out
    assert len(ctx.packet_queue_out[HB_PORT]) == 2


def test_hb_recv_fail(mock_heartbeat_checker):
    hbc, ctx = mock_heartbeat_checker
    hbc.HB_INIT_PERIOD = 200
    hbc.register_heartbeat_host(get_uuid("HeartbeatSender.mock_heartbeater"), "tcp://127.0.0.1:23456")
    timeout = 4
    while timeout > 0 and not hbc.get_failed():
        time.sleep(0.1)
        timeout -= 0.1
    assert hbc.get_failed()
