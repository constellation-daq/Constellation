#!/usr/bin/env python3
"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0
"""

import pytest
import time
from unittest.mock import MagicMock
import zmq

from constellation.cscp import CSCPMessageVerb, CommandTransmitter

from constellation.commandmanager import requestable, BaseCommandReceiver


mock_packet_queue_recv = []
mock_packet_queue_sender = []


# SIDE EFFECTS RECEIVER
def mock_sock_send_recv(payload, flags):
    """Append buf to queue."""
    mock_packet_queue_sender.append(payload)


def mock_sock_recv_multipart_recv(flags):
    """Pop entry from queue."""
    if not mock_packet_queue_recv:
        raise zmq.ZMQError("no mock data")
    # "pop all"
    r, mock_packet_queue_recv[:] = mock_packet_queue_recv[:], []
    return r


# SIDE EFFECTS SENDER
def mock_sock_send_sender(payload, flags):
    """Append buf to queue."""
    mock_packet_queue_recv.append(payload)


def mock_sock_recv_multipart_sender(flags):
    """Pop entry from queue."""
    if not mock_packet_queue_sender:
        raise zmq.ZMQError("no mock data")
    # "pop all"
    r, mock_packet_queue_sender[:] = mock_packet_queue_sender[:], []
    return r


# FIXTURES
@pytest.fixture
def mock_socket_recv():
    mock = MagicMock()
    mock = mock.return_value
    mock.send = MagicMock(side_effect=mock_sock_send_recv)
    mock.recv_multipart = MagicMock(side_effect=mock_sock_recv_multipart_recv)
    yield mock


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
def mock_cmdreceiver(mock_socket_recv):
    class CommandReceiver(BaseCommandReceiver):
        @requestable
        def get_state(self, msg):
            return "state", "good", None

        @requestable
        def fcnallowed(self, msg):
            return "allowed", "allowed passed", None

        def _fcnallowed_is_allowed(self, msg):
            return True

        @requestable
        def fcnnotallowed(self, msg):
            return "notallowed", "yes", None

        def _fcnnotallowed_is_allowed(self, msg):
            return False

    cr = CommandReceiver("mock_satellite", mock_socket_recv)
    yield cr


def test_chirp_beacon_send_recv(mock_socket_sender, mock_socket_recv):
    """Test self-concistency between two transmitters (sender/receiver)."""
    sender = CommandTransmitter("mock_sender", mock_socket_sender)
    receiver = CommandTransmitter("mock_receiver", mock_socket_recv)
    sender.send_request("make", "sandwich")
    req = receiver.get_message()
    assert req.msg == "make"
    assert req.payload == "sandwich"
    receiver.send_reply("no", CSCPMessageVerb.INVALID, "make your own sandwich")
    req = sender.get_message()
    assert req.msg == "no"
    assert "make your" in req.payload


def test_command_receiver(mock_cmdreceiver, mock_transmitter):
    """Test sending cmds and retrieving answers."""
    # cmd w/o '_is_allowed' method: always allowed
    mock_transmitter.send_request("get_state")
    # give the thread a chance to receive the message
    time.sleep(0.2)
    rep = mock_transmitter.get_message()
    assert rep.payload == "good"

    # cmd w/ '_is_allowed' method: always returns True
    mock_transmitter.send_request("fcnallowed")
    # give the thread a chance to receive the message
    time.sleep(0.2)
    rep = mock_transmitter.get_message()
    assert rep.payload == "allowed passed"

    # cmd w/ '_is_allowed' method: always returns False
    mock_transmitter.send_request("fcnnotallowed")
    # give the thread a chance to receive the message
    time.sleep(0.2)
    rep = mock_transmitter.get_message()
    assert rep.msg_verb == CSCPMessageVerb.INVALID
