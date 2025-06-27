"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2
"""

import threading
import time

import pytest
from conftest import wait_for_state

from constellation.core.configuration import Configuration
from constellation.core.message.cscp1 import CSCP1Message
from constellation.core.network import get_loopback_interface_name
from constellation.satellites.Mariner.Mariner import Mariner as MarinerDef


class Mariner(MarinerDef):
    def do_initializing(self, config: Configuration) -> str:
        # Overwrite and check existence of "voltage" before setting default for test
        config["voltage"]
        return super().do_initializing(config)


@pytest.fixture
def mariner_satellite(mock_zmq_context, mock_chirp_transmitter):
    """Mock a Satellite for a specific device, ie. a class inheriting from Satellite."""
    ctx = mock_zmq_context()
    ctx.flip_queues()

    s = Mariner("Nine", "mockstellation", 11111, 22222, 33333, [get_loopback_interface_name()])
    t = threading.Thread(target=s.run_satellite)
    t.start()
    # give the threads a chance to start
    time.sleep(0.1)
    yield s
    s.reentry()


# %%%%%%%%%%%%%%%
# TESTS
# %%%%%%%%%%%%%%%


def test_mariner_instantiation(mariner_satellite):
    """Test that we can create the satellite."""
    assert mariner_satellite.name == "Mariner.Nine"


def test_mariner_fsm_transition(mock_cmd_transmitter, mariner_satellite):
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
            payload = {"voltage": 1000, "current": 3, "sample_period": 1}
        elif cmd == "start":
            payload = "5001"
        else:
            # send a dict, why not?
            payload = {"mock key": "mock argument string"}
        sender.send_request(cmd, payload)
        time.sleep(0.2)
        req = sender.get_message()
        assert isinstance(req, CSCP1Message)
        assert "transitioning" in req.verb_msg.lower()
        assert req.verb_type == CSCP1Message.Type.SUCCESS
        # wait for state transition

        wait_for_state(mariner_satellite.fsm, state, 4.0)
        # check state
        sender.send_request("get_state")
        time.sleep(0.2)
        req = sender.get_message()
        assert isinstance(req, CSCP1Message)
        assert state.lower() in req.verb_msg.lower()
        assert req.verb_type == CSCP1Message.Type.SUCCESS


def test_mariner_cfg_missing_items(mock_cmd_transmitter, mariner_satellite):
    """Test that Satellite can gracefully handle missing cfg items."""
    sender = mock_cmd_transmitter
    payload = {"mock_cfg_key": "mock config string"}
    sender.send_request("initialize", payload)
    time.sleep(0.2)
    req = sender.get_message()
    assert isinstance(req, CSCP1Message)
    assert "transitioning" in req.verb_msg.lower()
    assert req.verb_type == CSCP1Message.Type.SUCCESS
    # wait for state transition

    wait_for_state(mariner_satellite.fsm, "ERROR", 4.0)
    assert "missing a required configuration value" in mariner_satellite.fsm.status


def test_mariner_command(mock_cmd_transmitter, mariner_satellite):
    """Test that Satellite can gracefully handle missing cfg items."""
    sender = mock_cmd_transmitter
    sender.send_request("get_attitude", None)
    time.sleep(0.2)
    req = sender.get_message()
    assert isinstance(req, CSCP1Message)
    assert "not ready" in req.verb_msg.lower()
    assert req.verb_type == CSCP1Message.Type.SUCCESS
    assert not req.payload

    payload = {"voltage": 1000, "current": 3, "sample_period": 1}
    sender.request_get_response("initialize", payload)
    time.sleep(0.2)

    sender.send_request("get_attitude", None)
    time.sleep(0.2)
    req = sender.get_message()
    assert isinstance(req, CSCP1Message)
    assert "locked and ready" in req.verb_msg.lower()
    assert req.verb_type == CSCP1Message.Type.SUCCESS
    assert isinstance(req.payload, int)
