"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2
"""

import time
from unittest.mock import MagicMock, patch

import pytest
import zmq
from conftest import mocket, send_port

from constellation.core.commandmanager import CommandReceiver, cscp_requestable
from constellation.core.cscp import CommandTransmitter, CSCPMessageVerb

CMD_PORT = send_port


@pytest.fixture
def mock_cmdreceiver(mock_chirp_transmitter):

    def mocket_factory(*args, **kwargs):
        m = mocket()
        m.endpoint = 0
        return m

    class MockCommandReceiver(CommandReceiver):
        @cscp_requestable
        def get_state(self, msg):
            return "state", "good", None

        @cscp_requestable
        def fcnallowed(self, msg):
            return "allowed", "allowed passed", None

        def _fcnallowed_is_allowed(self, msg):
            return True

        @cscp_requestable
        def fcnnotallowed(self, msg):
            return "notallowed", "yes", None

        def _fcnnotallowed_is_allowed(self, msg):
            return False

    with patch("constellation.core.commandmanager.zmq.Context") as mock:
        mock_context = MagicMock()
        mock_context.socket = mocket_factory
        mock.return_value = mock_context
        cr = MockCommandReceiver("mock_satellite", cmd_port=CMD_PORT, interface="127.0.0.1")
        cr._add_com_thread()
        cr._start_com_threads()
        # give the thread a chance to start
        time.sleep(0.1)
        yield cr


@pytest.mark.forked
def test_cmdtransmitter_send_recv(mock_socket_sender, mock_socket_receiver):
    """Test self-concistency between two transmitters (sender/receiver)."""
    sender = CommandTransmitter("mock_sender", mock_socket_sender)
    receiver = CommandTransmitter("mock_receiver", mock_socket_receiver)
    # send a request
    sender.send_request("make", "sandwich")
    req = receiver.get_message()
    assert req.msg == "make"
    assert req.payload == "sandwich"
    receiver.send_reply("no", CSCPMessageVerb.INVALID, "make your own sandwich")
    req = sender.get_message()
    assert req.msg == "no"
    assert "make your" in req.payload


@pytest.mark.forked
def test_cmdtransmitter_case_insensitve(mock_socket_sender, mock_socket_receiver):
    """Test that commands are received case insensitive (i.e. lower)."""
    sender = CommandTransmitter("mock_sender", mock_socket_sender)
    receiver = CommandTransmitter("mock_receiver", mock_socket_receiver)
    # send a request
    sender.send_request("MAKE", "Sandwich")
    req = receiver.get_message()
    assert req.msg == "make"
    assert req.payload == "Sandwich"


@pytest.mark.forked
def test_command_receiver(mock_cmdreceiver, mock_cmd_transmitter):
    """Test sending cmds and retrieving answers."""
    # cmd w/o '_is_allowed' method: always allowed
    mock_cmd_transmitter.send_request("get_state")
    # give the thread a chance to receive the message
    time.sleep(0.1)
    rep = mock_cmd_transmitter.get_message()
    assert rep.msg_verb == CSCPMessageVerb.SUCCESS
    assert rep.payload == "good"

    # cmd w/ '_is_allowed' method: always returns True
    mock_cmd_transmitter.send_request("fcnallowed")
    # give the thread a chance to receive the message
    time.sleep(0.1)
    rep = mock_cmd_transmitter.get_message()
    assert rep.msg_verb == CSCPMessageVerb.SUCCESS
    assert rep.payload == "allowed passed"

    # cmd w/ '_is_allowed' method: always returns False
    mock_cmd_transmitter.send_request("fcnnotallowed")
    # give the thread a chance to receive the message
    time.sleep(0.1)
    rep = mock_cmd_transmitter.get_message()
    assert rep.msg_verb == CSCPMessageVerb.INVALID
    assert not rep.payload


@pytest.mark.forked
def test_thread_shutdown(mock_cmdreceiver, mock_cmd_transmitter):
    """Test that receiver thread shuts down properly"""
    # shut down the thread
    mock_cmdreceiver._stop_com_threads()
    # send a request
    mock_cmd_transmitter.send_request("get_state")
    time.sleep(0.2)
    rep = mock_cmd_transmitter.get_message(zmq.NOBLOCK)
    # reply should be missing
    assert not rep
    assert not mock_cmdreceiver._com_thread_evt


@pytest.mark.forked
def test_cmd_unique_commands(mock_cmdreceiver):
    """Test that commands from different classes do not mix."""

    class MockOtherCommandReceiver(CommandReceiver):
        @cscp_requestable
        def get_unique_value(self, msg):
            return 42, None, None

    with patch("constellation.core.commandmanager.zmq.Context"):
        cr = MockOtherCommandReceiver("mock_other_satellite", cmd_port=22222, interface="127.0.0.1")
        msg, cmds, _meta = cr.get_commands()
        # get_state not part of MockOtherCommandReceiver but of MockCommandReceiver
        assert "get_state" not in cmds
