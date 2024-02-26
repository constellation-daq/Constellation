#!/usr/bin/env python3
"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0
"""

import pytest
import time
import threading
from unittest.mock import MagicMock, patch
import zmq

from constellation.cscp import CSCPMessageVerb, CommandTransmitter

from constellation.satellite import Satellite


mock_packet_queue_recv = {}
mock_packet_queue_sender = {}
send_port = 11111


# SIDE EFFECTS
class mocket:
    """Mock socket for a receiver."""

    def __init__(self):
        self.port = 0

    # receiver
    def send(self, payload, flags=None):
        """Append buf to queue."""
        try:
            mock_packet_queue_sender[self.port].append(payload)
        except KeyError:
            mock_packet_queue_sender[self.port] = [payload]

    def send_string(self, payload, flags=None):
        self.send(payload)

    def recv_multipart(self, flags=None):
        """Pop entry from queue."""
        if (
            self.port not in mock_packet_queue_recv
            or not mock_packet_queue_recv[self.port]
        ):
            raise zmq.ZMQError("no mock data")
        # "pop all"
        r, mock_packet_queue_recv[self.port][:] = (
            mock_packet_queue_recv[self.port][:],
            [],
        )
        return r

    def bind(self, host):
        self.port = int(host.split(":")[2])


# SIDE EFFECTS SENDER
def mock_sock_send_sender(payload, flags=None):
    """Append buf to queue."""
    try:
        mock_packet_queue_recv[send_port].append(payload)
    except KeyError:
        mock_packet_queue_recv[send_port] = [payload]


def mock_sock_recv_multipart_sender(flags=None):
    """Pop entry from queue."""
    if (
        send_port not in mock_packet_queue_sender
        or not mock_packet_queue_sender[send_port]
    ):
        raise zmq.ZMQError("no mock data")
    # "pop all"
    r, mock_packet_queue_sender[send_port][:] = (
        mock_packet_queue_sender[send_port][:],
        [],
    )
    return r


@pytest.fixture
def mock_socket_sender():
    mock = MagicMock()
    mock = mock.return_value
    mock.send = MagicMock(side_effect=mock_sock_send_sender)
    mock.recv_multipart = MagicMock(side_effect=mock_sock_recv_multipart_sender)
    yield mock


@pytest.fixture
def mock_transmitter(mock_socket_sender):
    t = CommandTransmitter("mock_sender", mock_socket_sender)
    yield t


@pytest.fixture
def mock_satellite():
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
def test_satellite_fsm_change_on_cmd(mock_socket_sender, mock_satellite):
    """Test cmd reception."""
    sender = CommandTransmitter("mock_sender", mock_socket_sender)
    # send a request
    sender.send_request("get_state")
    time.sleep(0.2)
    req = sender.get_message()
    assert "new" in req.msg.lower()
    assert req.msg_verb == CSCPMessageVerb.SUCCESS
    # transition
    sender.send_request("transition", "initialize")
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


# TODO test shutdown
# TODO test wrong packet (header)
# TODO test state transitions
