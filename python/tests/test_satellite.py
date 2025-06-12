"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2
"""

import threading
import time
from unittest.mock import MagicMock, patch

import pytest
from conftest import mocket, wait_for_state

from constellation.core import __version__
from constellation.core.broadcastmanager import DiscoveredService, chirp_callback
from constellation.core.chirp import CHIRPMessageType, CHIRPServiceIdentifier
from constellation.core.chp import CHPRole
from constellation.core.cscp import CommandTransmitter
from constellation.core.fsm import SatelliteState
from constellation.core.message.cscp1 import CSCP1Message
from constellation.core.network import get_loopback_interface_name
from constellation.core.satellite import Satellite


@pytest.fixture
def mock_device_satellite(mock_chirp_socket):
    """Mock a Satellite for a specific device, ie. a class inheriting from Satellite."""

    def mocket_factory(*args, **kwargs):
        m = mocket()
        return m

    class MockDeviceSatellite(Satellite):
        callback_triggered = False
        KEEP_WAITING = True

        @chirp_callback(CHIRPServiceIdentifier.DATA)
        def callback_function(self, service):
            self.callback_triggered = service

        def do_initializing(self, payload):
            self.KEEP_WAITING = True
            while self.KEEP_WAITING:
                time.sleep(0.01)
            return "finished with mock initialization"

        def ready(self):
            self.KEEP_WAITING = False

    with patch("constellation.core.base.zmq.Context") as mock:
        mock_context = MagicMock()
        mock_context.socket = mocket_factory
        mock.return_value = mock_context
        s = MockDeviceSatellite("mydevice1", "mockstellation", 11111, 22222, 33333, [get_loopback_interface_name()])
        t = threading.Thread(target=s.run_satellite)
        t.start()
        # give the threads a chance to start
        time.sleep(0.1)
        yield s


@pytest.fixture
def mock_fail_satellite(mock_chirp_transmitter):
    """Mock a Satellite that fails on run."""

    def mocket_factory(*args, **kwargs):
        m = mocket()
        return m

    class MockFailSatellite(Satellite):

        def do_run(self, run_identifier):
            raise RuntimeError("mock failure")

    with patch("constellation.core.base.zmq.Context") as mock:
        mock_context = MagicMock()
        mock_context.socket = mocket_factory
        mock.return_value = mock_context
        s = MockFailSatellite("fail1", "mockstellation", 11111, 22222, 33333, [get_loopback_interface_name()])
        t = threading.Thread(target=s.run_satellite)
        t.start()
        # give the threads a chance to start
        time.sleep(0.1)
        yield s


# %%%%%%%%%%%%%%%
# TESTS
# %%%%%%%%%%%%%%%


@pytest.mark.forked
def test_device_satellite_instantiation(mock_device_satellite):
    """Test that we can create the satellite."""
    assert mock_device_satellite.name == "MockDeviceSatellite.mydevice1"


@pytest.mark.forked
def test_satellite_unknown_cmd_recv(mock_socket_sender, mock_satellite):
    """Test unknown cmd reception."""
    sender = CommandTransmitter("mock_sender", mock_socket_sender)
    # send a request
    sender.send_request("make", "sandwich")
    time.sleep(0.2)
    req = sender.get_message()
    assert isinstance(req, CSCP1Message)
    assert "unknown" in req.verb_msg.lower()
    assert req.verb_type == CSCP1Message.Type.UNKNOWN


@pytest.mark.forked
def test_satellite_std_commands(mock_socket_sender, mock_satellite):
    sender = CommandTransmitter("mock_sender", mock_socket_sender)
    # get_name
    req = sender.request_get_response("get_name")
    assert isinstance(req, CSCP1Message)
    assert req.verb_type == CSCP1Message.Type.SUCCESS
    assert req.verb_msg == "Satellite.mock_satellite"
    # get_version
    req = sender.request_get_response("get_version")
    assert isinstance(req, CSCP1Message)
    assert req.verb_type == CSCP1Message.Type.SUCCESS
    assert req.verb_msg == __version__
    # get_commands
    req = sender.request_get_response("get_commands")
    assert isinstance(req, CSCP1Message)
    assert req.verb_type == CSCP1Message.Type.SUCCESS
    assert isinstance(req.payload, dict)
    # get_state
    req = sender.request_get_response("get_state")
    assert isinstance(req, CSCP1Message)
    assert req.verb_type == CSCP1Message.Type.SUCCESS
    assert req.verb_msg == "NEW"
    # get_role
    req = sender.request_get_response("get_role")
    assert isinstance(req, CSCP1Message)
    assert req.verb_type == CSCP1Message.Type.SUCCESS
    assert req.verb_msg == "DYNAMIC"
    assert req.payload == CHPRole.DYNAMIC.flags()
    # get_status
    req = sender.request_get_response("get_status")
    assert isinstance(req, CSCP1Message)
    assert req.verb_type == CSCP1Message.Type.SUCCESS
    # get_config
    req = sender.request_get_response("get_config")
    assert isinstance(req, CSCP1Message)
    assert req.verb_type == CSCP1Message.Type.SUCCESS
    assert isinstance(req.payload, dict)
    # get_run_id
    req = sender.request_get_response("get_run_id")
    assert isinstance(req, CSCP1Message)
    assert req.verb_type == CSCP1Message.Type.SUCCESS


@pytest.mark.forked
def test_satellite_wrong_type_payload(mock_socket_sender, mock_satellite):
    """Test unknown cmd reception."""
    sender = CommandTransmitter("mock_sender", mock_socket_sender)
    # send a request with wrong type payload
    sender.send_request("start", 12233.0)
    time.sleep(0.2)
    req = sender.get_message()
    assert isinstance(req, CSCP1Message)
    assert "wrong argument" in req.verb_msg.lower()
    assert req.verb_type == CSCP1Message.Type.INCOMPLETE
    # send a request with wrong type payload
    sender.send_request("initialize", 12233.0)
    time.sleep(0.2)
    req = sender.get_message()
    assert isinstance(req, CSCP1Message)
    assert "wrong argument" in req.verb_msg.lower()
    assert req.verb_type == CSCP1Message.Type.INCOMPLETE
    # send a request with wrong type payload
    sender.send_request("initialize", None)
    time.sleep(0.2)
    req = sender.get_message()
    assert isinstance(req, CSCP1Message)
    assert "wrong argument" in req.verb_msg.lower()
    assert req.verb_type == CSCP1Message.Type.INCOMPLETE


@pytest.mark.forked
def test_satellite_fsm_change_on_cmd(mock_cmd_transmitter, mock_satellite):
    """Test fsm state change cmd reception."""
    sender = mock_cmd_transmitter
    # send a request
    sender.send_request("get_state")
    time.sleep(0.2)
    req = sender.get_message()
    assert isinstance(req, CSCP1Message)
    assert req.verb_msg == "NEW"
    assert req.verb_type == CSCP1Message.Type.SUCCESS
    # transition
    sender.send_request("initialize", {"mock key": "mock argument string"})
    time.sleep(0.2)
    req = sender.get_message()
    assert isinstance(req, CSCP1Message)
    assert req.verb_msg == "transitioning"
    assert req.verb_type == CSCP1Message.Type.SUCCESS
    # check state
    sender.send_request("get_state")
    time.sleep(0.2)
    req = sender.get_message()
    assert isinstance(req, CSCP1Message)
    assert req.verb_msg == "INIT"
    assert req.verb_type == CSCP1Message.Type.SUCCESS


@pytest.mark.forked
def test_satellite_fsm_change_transitional(mock_cmd_transmitter, mock_device_satellite):
    """Test slow transitional states."""
    satellite = mock_device_satellite
    sender = mock_cmd_transmitter
    # send a request to init
    sender.send_request("initialize", {"mock key": "mock argument string"})
    time.sleep(0.1)
    req = sender.get_message()
    assert isinstance(req, CSCP1Message)
    assert req.verb_msg == "transitioning"
    assert req.verb_type == CSCP1Message.Type.SUCCESS
    # check state
    sender.send_request("get_state")
    time.sleep(0.1)
    req = sender.get_message()
    assert isinstance(req, CSCP1Message)
    assert req.verb_msg == "initializing"
    assert req.verb_type == CSCP1Message.Type.SUCCESS
    satellite.ready()
    # check state again
    time.sleep(0.1)
    sender.send_request("get_state")
    time.sleep(0.1)
    req = sender.get_message()
    assert isinstance(req, CSCP1Message)
    assert req.verb_msg == "INIT"
    assert req.verb_type == CSCP1Message.Type.SUCCESS


@pytest.mark.forked
def test_satellite_fsm_cannot_change_transitional(mock_cmd_transmitter, mock_device_satellite):
    """Test transitions from slow transitional states."""
    sender = mock_cmd_transmitter
    # send a request to init
    sender.send_request("initialize", {"mock key": "mock argument string"})
    time.sleep(0.1)
    req = sender.get_message()
    assert isinstance(req, CSCP1Message)
    assert "transitioning" in str(req.verb_msg).lower()
    assert req.verb_type == CSCP1Message.Type.SUCCESS
    # check state
    sender.send_request("get_state")
    time.sleep(0.1)
    req = sender.get_message()
    assert isinstance(req, CSCP1Message)
    assert "initializing" in str(req.verb_msg).lower()
    assert req.verb_type == CSCP1Message.Type.SUCCESS
    # send a request to init again
    sender.send_request("initialize", {"mock key": "mock argument string"})
    time.sleep(0.1)
    req = sender.get_message()
    assert isinstance(req, CSCP1Message)
    assert req.verb_type == CSCP1Message.Type.INVALID
    # send a request to finish translational state
    sender.send_request("initialized")  # not a command
    time.sleep(0.2)
    req = sender.get_message()
    assert isinstance(req, CSCP1Message)
    assert req.verb_type == CSCP1Message.Type.UNKNOWN
    # send a request to move to next state before we are ready
    sender.send_request("launch")  # not allowed
    time.sleep(0.2)
    req = sender.get_message()
    assert isinstance(req, CSCP1Message)
    assert req.verb_type == CSCP1Message.Type.INVALID


@pytest.mark.forked
def test_satellite_chirp_offer(mock_chirp_transmitter, mock_device_satellite):
    """Test cmd reception."""
    satellite = mock_device_satellite
    assert not satellite.callback_triggered
    mock_chirp_transmitter.broadcast(CHIRPServiceIdentifier.DATA, CHIRPMessageType.OFFER, 666)
    time.sleep(0.5)
    # chirp message has been processed
    assert satellite._beacon._socket._recv_socket.seen >= 1
    assert satellite.callback_triggered
    assert isinstance(satellite.callback_triggered, DiscoveredService)


@pytest.mark.forked
def test_satellite_fsm_transition_walk(mock_cmd_transmitter, mock_satellite):
    """Test that Satellite can 'walk' through a series of transitions."""
    transitions = {
        "initialize": "INIT",
        "launch": "ORBIT",
        "start": "RUN",
        "stop": "ORBIT",
        "land": "INIT",
    }
    sender = mock_cmd_transmitter
    for cmd, state in transitions.items():
        if cmd == "initialize":
            payload = {"mock_cfg_key": "mock config string"}
        elif cmd == "start":
            payload = "5001"
        else:
            # send a dict, why not?
            payload = {"mock key": "mock argument string"}
        sender.send_request(cmd, payload)
        time.sleep(0.2)
        req = sender.get_message()
        assert isinstance(req, CSCP1Message)
        assert "transitioning" in str(req.verb_msg).lower()
        assert req.verb_type == CSCP1Message.Type.SUCCESS
        # wait for state transition

        wait_for_state(mock_satellite.fsm, state, 4.0)
        # check state
        sender.send_request("get_state")
        time.sleep(0.2)
        req = sender.get_message()
        assert isinstance(req, CSCP1Message)
        assert state.lower() in str(req.verb_msg).lower()
        assert req.verb_type == CSCP1Message.Type.SUCCESS


@pytest.mark.forked
def test_satellite_fsm_timestamp(mock_cmd_transmitter, mock_satellite):
    """Test that FSM timestamps transitions."""
    transitions = {
        "initialize": "INIT",
        "launch": "ORBIT",
        "start": "RUN",
        "stop": "ORBIT",
        "land": "INIT",
    }
    sender = mock_cmd_transmitter
    assert mock_satellite.fsm.last_changed
    last_changed = mock_satellite.fsm.last_changed
    for cmd, state in transitions.items():
        if cmd == "initialize":
            payload = {"mock_cfg_key": "mock config string"}
        elif cmd == "start":
            payload = "5001"
        else:
            # send a dict, why not?
            payload = {"mock key": "mock argument string"}
        sender.send_request(cmd, payload)
        time.sleep(0.2)
        req = sender.get_message()
        assert isinstance(req, CSCP1Message)
        assert "transitioning" in str(req.verb_msg).lower()
        assert req.verb_type == CSCP1Message.Type.SUCCESS
        # wait for state transition

        wait_for_state(mock_satellite.fsm, state, 4.0)
        # check state
        assert (mock_satellite.fsm.last_changed - last_changed).total_seconds() > 0
        last_changed = mock_satellite.fsm.last_changed
        sender.send_request("get_state")
        time.sleep(0.2)
        req = sender.get_message()
        assert isinstance(req, CSCP1Message)
        assert state.lower() in str(req.verb_msg).lower()
        assert req.verb_type == CSCP1Message.Type.SUCCESS
        assert req.tags["last_changed"] == last_changed
        assert req.tags["last_changed_iso"] == last_changed.isoformat()
        assert req.payload == getattr(SatelliteState, state).value


@pytest.mark.forked
def test_satellite_run_id_cmd(mock_cmd_transmitter, mock_satellite):
    """Test that FSM timestamps transitions."""
    transitions = {
        "initialize": "INIT",
        "launch": "ORBIT",
        "start": "RUN",
        "stop": "ORBIT",
        "land": "INIT",
    }
    sender = mock_cmd_transmitter
    run_id = ""
    assert mock_satellite.run_identifier == run_id

    for cmd, state in transitions.items():
        if cmd == "initialize":
            payload = {"mock_cfg_key": "mock config string"}
        elif cmd == "start":
            payload = "5001"
            run_id = "5001"
        else:
            # send a dict, why not?
            payload = {"mock key": "mock argument string"}
        req = sender.request_get_response(cmd, payload)
        assert isinstance(req, CSCP1Message)
        assert "transitioning" in str(req.verb_msg).lower()
        assert req.verb_type == CSCP1Message.Type.SUCCESS
        # wait for state transition
        wait_for_state(mock_satellite.fsm, state, 4.0)
        # check run id
        assert mock_satellite.run_identifier == run_id
        req = sender.request_get_response("get_run_id")
        assert req.verb_msg == run_id


@pytest.mark.forked
def test_satellite_run_fail(mock_cmd_transmitter, mock_fail_satellite):
    """Test that Satellite can fail in run."""
    transitions = {
        "initialize": "INIT",
        "launch": "ORBIT",
        "start": "RUN",
    }
    sender = mock_cmd_transmitter
    for cmd, state in transitions.items():
        if cmd == "initialize":
            payload = {"mock_cfg_key": "mock config string"}
        elif cmd == "start":
            payload = "5001"
        else:
            # send a dict, why not?
            payload = {"mock key": "mock argument string"}
        sender.send_request(cmd, payload)
        time.sleep(0.2)
        req = sender.get_message()
        assert isinstance(req, CSCP1Message)
        assert "transitioning" in str(req.verb_msg).lower()
        assert req.verb_type == CSCP1Message.Type.SUCCESS
        # wait for state transition; should fail for RUN
        if state == "RUN":
            state = "ERROR"
        wait_for_state(mock_fail_satellite.fsm, state, 1.0)
        # check state
        sender.send_request("get_state")
        time.sleep(0.2)
        req = sender.get_message()
        assert isinstance(req, CSCP1Message)
        assert state.lower() in str(req.verb_msg).lower()
        assert req.verb_type == CSCP1Message.Type.SUCCESS
