#!/usr/bin/env python3
"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0
"""

import pytest
import time
import threading
from unittest.mock import MagicMock, patch

from constellation.cscp import CSCPMessageVerb, CommandTransmitter

from constellation.satellite import Satellite

from constellation.broadcastmanager import (
    chirp_callback,
    DiscoveredService,
)

from constellation.chirp import (
    CHIRPMessageType,
    CHIRPServiceIdentifier,
)

from conftest import mock_chirp_packet_queue, mocket


@pytest.fixture
def mock_satellite():
    """Create a mock Satellite base instance."""

    def mocket_factory(*args, **kwargs):
        m = mocket()
        return m

    with patch("constellation.base.zmq.Context") as mock:
        mock_context = MagicMock()
        mock_context.socket = mocket_factory
        mock.return_value = mock_context
        s = Satellite("mock_satellite", "mockstellation", 11111, 22222, 33333)
        t = threading.Thread(target=s.run_satellite)
        t.start()
        # give the threads a chance to start
        time.sleep(0.1)
        yield s


@pytest.fixture
def mock_device_satellite(mock_chirp_socket):
    """Mock a Satellite for a specific device, ie. a class inheriting from Satellite."""

    def mocket_factory(*args, **kwargs):
        m = mocket()
        return m

    class MockDeviceSatellite(Satellite):
        callback_triggered = False

        @chirp_callback(CHIRPServiceIdentifier.DATA)
        def callback_function(self, service):
            self.callback_triggered = service

    with patch("constellation.base.zmq.Context") as mock:
        mock_context = MagicMock()
        mock_context.socket = mocket_factory
        mock.return_value = mock_context
        s = MockDeviceSatellite("mock_satellite", "mockstellation", 11111, 22222, 33333)
        t = threading.Thread(target=s.run_satellite)
        t.start()
        # give the threads a chance to start
        time.sleep(0.1)
        yield s


# %%%%%%%%%%%%%%%
# TESTS
# %%%%%%%%%%%%%%%


@pytest.mark.forked
def test_satellite_unknown_cmd_recv(mock_socket_sender, mock_satellite):
    """Test cmd reception."""
    sender = CommandTransmitter("mock_sender", mock_socket_sender)
    # send a request
    sender.send_request("make", "sandwich")
    time.sleep(0.2)
    req = sender.get_message()
    assert "unknown" in req.msg.lower()
    assert req.msg_verb == CSCPMessageVerb.UNKNOWN


@pytest.mark.forked
def test_satellite_fsm_change_on_cmd(mock_cmd_transmitter, mock_satellite):
    """Test cmd reception."""
    sender = mock_cmd_transmitter
    # send a request
    sender.send_request("get_state")
    time.sleep(0.2)
    req = sender.get_message()
    assert "new" in req.msg.lower()
    assert req.msg_verb == CSCPMessageVerb.SUCCESS
    # transition
    sender.send_request("initialize", "mock argument string")
    time.sleep(0.2)
    req = sender.get_message()
    assert "transitioning" in req.msg.lower()
    assert req.msg_verb == CSCPMessageVerb.SUCCESS
    # check state
    sender.send_request("get_state")
    time.sleep(0.2)
    req = sender.get_message()
    assert "init" in req.msg.lower()
    assert req.msg_verb == CSCPMessageVerb.SUCCESS


@pytest.mark.forked
def test_satellite_chirp_offer(mock_chirp_transmitter, mock_device_satellite):
    """Test cmd reception."""
    assert not mock_device_satellite.callback_triggered
    mock_chirp_transmitter.broadcast(
        CHIRPServiceIdentifier.DATA, CHIRPMessageType.OFFER, 666
    )
    time.sleep(0.5)
    # chirp message has been processed
    assert len(mock_chirp_packet_queue) == 0
    assert mock_device_satellite.callback_triggered
    assert isinstance(mock_device_satellite.callback_triggered, DiscoveredService)


# TODO test shutdown
# TODO test wrong packet (header)
# TODO test state transitions
