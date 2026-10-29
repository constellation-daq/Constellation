"""
SPDX-FileCopyrightText: 2026 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2

Tests for AsyncSatellite FSM transitions, commands, reconfigure,
failure and interrupt handling.
"""

import asyncio
import random
import threading
import time
from functools import partial
from unittest.mock import Mock, patch

import pytest
import zmq

from constellation.core.async_experimental.async_satellite import AsyncSatellite
from constellation.core.cscp import CommandTransmitter
from constellation.core.message.cscp1 import CSCP1Message
from constellation.core.network import get_loopback_interface_name
from constellation.core.protocol.cscp1 import SatelliteState

SNDMORE_MARK = "_S/END_"
MON_PORT = 33333


# ---------------------------------------------------------------------------
# Mock sockets
# ---------------------------------------------------------------------------


class _BaseMocket:
    """Shared mocket logic for both sync and async mock sockets."""

    def __init__(self):
        self.port = 0
        self.packet_queue_in = {}
        self.packet_queue_out = {}
        self.closed = False

    def _get_queue(self, out: bool):
        return self.packet_queue_out if out else self.packet_queue_in

    def _create_queue(self):
        self._get_queue(True)[self.port] = []
        self._get_queue(False)[self.port] = []

    def _do_send(self, payload, flags=None):
        q = self._get_queue(True)
        if self.port not in q:
            q[self.port] = []
        if isinstance(flags, zmq.Flag) and zmq.SNDMORE & flags:
            q[self.port].append(payload)
        else:
            q[self.port].append([payload, SNDMORE_MARK])

    def _do_recv_multipart(self):
        r = []
        while True:
            dat = self._get_queue(False)[self.port].pop(0)
            if isinstance(dat, list) and SNDMORE_MARK in dat:
                r.append(dat[0])
                break
            r.append(dat)
        return r

    def has_no_data(self):
        return self.port not in self._get_queue(False) or not self._get_queue(False)[self.port]

    def bind(self, host):
        self.port = int(host.split(":")[2])
        self._create_queue()

    def bind_to_random_port(self, host):
        self.port = random.randrange(10000, 55555)
        self._create_queue()
        return self.port

    def connect(self, host):
        self.port = int(host.split(":")[2])
        self._create_queue()

    def setsockopt(self, *args, **kwargs):
        pass

    def setsockopt_string(self, option, value, *args, **kwargs):
        pass

    def close(self, *args, **kwargs):
        self.closed = True


class sync_mocket(_BaseMocket):
    """Sync mock ZMQ socket for monitoring and heartbeat contexts."""

    def send(self, payload, flags=None):
        self._do_send(payload, flags)

    def send_string(self, payload, flags=None):
        self._do_send(payload.encode(), flags=flags)

    def send_multipart(self, msg_parts, flags=None):
        for idx, msg in enumerate(msg_parts):
            flag = zmq.SNDMORE if idx < len(msg_parts) - 1 else None
            self._do_send(msg, flag)

    def recv_multipart(self, flags=None):
        if flags == zmq.NOBLOCK:
            if self.has_no_data():
                raise zmq.ZMQError(msg="Resource temporarily unavailable")
        else:
            while self.has_no_data():
                time.sleep(0.01)
        return self._do_recv_multipart()

    def recv(self, flags=None):
        if flags == zmq.NOBLOCK:
            if self.has_no_data():
                raise zmq.ZMQError(msg="Resource temporarily unavailable")
            return self._get_queue(False)[self.port].pop(0)
        while self.has_no_data():
            time.sleep(0.01)
        dat = self._get_queue(False)[self.port].pop(0)
        if isinstance(dat, list) and SNDMORE_MARK in dat:
            return dat[0]
        return dat


class async_mocket(_BaseMocket):
    """Async mock ZMQ socket for CSCP and heartbeat sender contexts."""

    async def send(self, payload, flags=None):
        self._do_send(payload, flags)

    async def send_string(self, payload, flags=None):
        self._do_send(payload.encode(), flags=flags)

    async def send_multipart(self, msg_parts, flags=None):
        for idx, msg in enumerate(msg_parts):
            flag = zmq.SNDMORE if idx < len(msg_parts) - 1 else None
            self._do_send(msg, flag)

    async def recv_multipart(self, flags=None):
        while self.has_no_data():
            await asyncio.sleep(0.01)
        return self._do_recv_multipart()

    async def recv(self, flags=None):
        if flags == zmq.NOBLOCK:
            if self.has_no_data():
                raise zmq.ZMQError(msg="Resource temporarily unavailable")
            dat = self._get_queue(False)[self.port].pop(0)
            if isinstance(dat, list) and SNDMORE_MARK in dat:
                return dat[0]
            return dat
        while self.has_no_data():
            await asyncio.sleep(0.01)
        dat = self._get_queue(False)[self.port].pop(0)
        if isinstance(dat, list) and SNDMORE_MARK in dat:
            return dat[0]
        return dat

    async def poll(self, timeout=500, flags=zmq.POLLIN):
        deadline = time.monotonic() + timeout / 1000.0
        while self.has_no_data():
            if time.monotonic() >= deadline:
                return 0
            await asyncio.sleep(0.01)
        return flags


# ---------------------------------------------------------------------------
# Fixtures
# ---------------------------------------------------------------------------


@pytest.fixture
def mock_async_zmq_context():
    """Mock ZMQ context factory for async satellites.

    Sync zmq.Context produces sync_mocket instances for monitoring
    and heartbeat. zmq.asyncio.Context produces async_mocket instances
    for the CSCP receiver and heartbeat sender.
    """
    packet_queue_in = {}
    packet_queue_out = {}

    def sync_factory(ctx, *args, **kwargs):
        m = sync_mocket()
        m.packet_queue_in = ctx.packet_queue_in
        m.packet_queue_out = ctx.packet_queue_out
        ctx._known_sockets.append(m)
        return m

    def async_factory(ctx, *args, **kwargs):
        m = async_mocket()
        m.packet_queue_in = ctx.packet_queue_in
        m.packet_queue_out = ctx.packet_queue_out
        ctx._known_sockets.append(m)
        return m

    with patch("constellation.core.heartbeatchecker.zmq.Context") as hbcontext:
        with patch("constellation.core.base.zmq.Context") as basecontext:
            with patch("constellation.core.commandmanager.zmq.Context") as cmdcontext:
                with patch("constellation.core.base.zmq.asyncio.Context") as asynccontext:

                    def context_factory():
                        sync_ctx = Mock()
                        async_ctx = Mock()

                        hbcontext.return_value = sync_ctx
                        basecontext.return_value = sync_ctx
                        cmdcontext.return_value = sync_ctx
                        asynccontext.return_value = async_ctx

                        for c in [hbcontext, basecontext, cmdcontext, asynccontext]:
                            c.cscp_command = False
                        sync_ctx.cscp_command = False
                        async_ctx.cscp_command = False

                        for c in [sync_ctx, async_ctx]:
                            c.packet_queue_in = packet_queue_in
                            c.packet_queue_out = packet_queue_out
                            c.endpoint = 0
                            c._known_sockets = []
                            c.term = Mock()

                        sync_ctx.socket = partial(sync_factory, sync_ctx)
                        async_ctx.socket = partial(async_factory, async_ctx)

                        return sync_ctx

                    yield context_factory


@pytest.fixture
def mock_async_cmd_transmitter(mock_async_zmq_context):
    """Sync transmitter for sending CSCP commands to the async satellite."""
    ctx = mock_async_zmq_context()
    s = sync_mocket()
    s.packet_queue_out = ctx.packet_queue_in
    s.packet_queue_in = ctx.packet_queue_out
    s.port = 11111
    s._create_queue()
    t = CommandTransmitter("mock_sender", s)
    yield t
    t.close()


def wait_for_state(fsm, state_name: str, timeout: float = 4.0):
    """Block until FSM reaches the given state or timeout."""
    target = SatelliteState[state_name.upper()]
    while timeout > 0 and fsm.state != target:
        time.sleep(0.01)
        timeout -= 0.01
    if timeout < 0:
        raise RuntimeError(f"Never reached {state_name}, got {fsm.state.name} with status '{fsm.status}'")


class MockAsyncDeviceSatellite(AsyncSatellite):
    """Mock device satellite for testing."""

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.init_called = False
        self.run_called = False
        self.stop_called = False
        self.reconfigured = False

    def emit_offers(self):
        pass

    def request(self, *args, **kwargs):
        pass

    async def _run_chirp(self, stop):
        await stop.wait()

    def do_initializing(self, config):
        self.init_called = True
        self.voltage = config.get("voltage", 5)
        return "mock initialized"

    def do_run(self):
        self.run_called = True
        while not self.stop_requested():
            time.sleep(0.01)
        return "mock run finished"

    def do_stopping(self):
        self.stop_called = True
        return "mock stopped"

    def do_reconfigure(self, partial_config):
        partial_config.get("voltage", 0)
        self.reconfigured = True
        return "mock reconfigured"


class MockAsyncFailSatellite(AsyncSatellite):
    """Mock satellite that raises in do_run."""

    def emit_offers(self):
        pass

    def request(self, *args, **kwargs):
        pass

    async def _run_chirp(self, stop):
        await stop.wait()

    def do_initializing(self, config):
        return "initialized"

    def do_run(self):
        raise RuntimeError("intentional test failure")


def _start_satellite(sat):
    """Start a satellite in a background thread."""
    loop = asyncio.new_event_loop()

    async def _run():
        await sat.run_satellite()

    thread = threading.Thread(target=loop.run_until_complete, args=(_run(),), daemon=True)
    thread.start()
    time.sleep(0.3)
    return thread, loop


@pytest.fixture
def async_satellite_fixture(mock_async_zmq_context):
    """Create a MockAsyncDeviceSatellite and run it in a background thread."""
    mock_async_zmq_context()

    with patch("constellation.core.multicast.socket.socket"):
        sat = MockAsyncDeviceSatellite(
            "mock_async_sat",
            "mockstellation",
            cmd_port=11111,
            hb_port=22222,
            mon_port=MON_PORT,
            interface=[get_loopback_interface_name()],
        )

    thread, loop = _start_satellite(sat)

    yield sat

    sat._async_task_queue.put_nowait(None)
    time.sleep(0.5)


@pytest.fixture
def async_fail_satellite_fixture(mock_async_zmq_context):
    """Create a MockAsyncFailSatellite and run it in a background thread."""
    mock_async_zmq_context()

    with patch("constellation.core.multicast.socket.socket"):
        sat = MockAsyncFailSatellite(
            "mock_fail_sat",
            "mockstellation",
            cmd_port=11111,
            hb_port=22222,
            mon_port=MON_PORT,
            interface=[get_loopback_interface_name()],
        )

    thread, loop = _start_satellite(sat)

    yield sat

    sat._async_task_queue.put_nowait(None)
    time.sleep(0.5)


# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------


def test_async_satellite_initial_state(async_satellite_fixture):
    """Verify the satellite starts in NEW state."""
    sat = async_satellite_fixture
    assert sat.fsm.state == SatelliteState.NEW


def test_async_satellite_get_state(mock_async_cmd_transmitter, async_satellite_fixture):
    """Verify get_state returns the current FSM state."""
    sender = mock_async_cmd_transmitter
    _ = async_satellite_fixture  # noqa: F841

    sender.send_request("get_state")
    time.sleep(0.5)
    req = sender.get_message()
    assert isinstance(req, CSCP1Message)
    assert req.verb_type == CSCP1Message.Type.SUCCESS
    assert "new" in str(req.verb_msg).lower()


def test_async_satellite_get_version(mock_async_cmd_transmitter, async_satellite_fixture):
    """Verify get_version returns a version string."""
    sender = mock_async_cmd_transmitter
    _ = async_satellite_fixture  # noqa: F841

    sender.send_request("get_version")
    time.sleep(0.5)
    req = sender.get_message()
    assert isinstance(req, CSCP1Message)
    assert req.verb_type == CSCP1Message.Type.SUCCESS


def test_async_satellite_initialize(mock_async_cmd_transmitter, async_satellite_fixture):
    """Verify the satellite can transition through initializing to INIT."""
    sender = mock_async_cmd_transmitter
    sat = async_satellite_fixture

    sender.send_request("initialize", {"key": "value"})
    time.sleep(0.3)
    req = sender.get_message()
    assert isinstance(req, CSCP1Message)
    assert req.verb_type == CSCP1Message.Type.SUCCESS
    assert "transitioning" in str(req.verb_msg).lower()

    wait_for_state(sat.fsm, "INIT", 4.0)
    assert sat.init_called


def test_async_satellite_fsm_walk(mock_async_cmd_transmitter, async_satellite_fixture):
    """Walk through init, launch, start, run, stop, land."""
    sender = mock_async_cmd_transmitter
    sat = async_satellite_fixture

    transitions = [
        ("initialize", "INIT", {"key": "value"}),
        ("launch", "ORBIT", None),
        ("start", "RUN", "5001"),
        ("stop", "ORBIT", None),
        ("land", "INIT", None),
    ]

    for cmd, expected_state, payload in transitions:
        sender.send_request(cmd, payload)
        time.sleep(0.3)
        req = sender.get_message()
        assert isinstance(req, CSCP1Message), f"No response for {cmd}"
        assert req.verb_type == CSCP1Message.Type.SUCCESS, f"{cmd} failed: {req.verb_type}: {req.verb_msg}"

        wait_for_state(sat.fsm, expected_state, 4.0)
        assert sat.fsm.state == SatelliteState[expected_state]


def test_async_satellite_reconfigure(mock_async_cmd_transmitter, async_satellite_fixture):
    """Verify reconfigure works from ORBIT state."""
    sender = mock_async_cmd_transmitter
    sat = async_satellite_fixture

    for cmd, payload in [("initialize", {"voltage": 5}), ("launch", None)]:
        sender.send_request(cmd, payload)
        time.sleep(0.3)
        sender.get_message()

    wait_for_state(sat.fsm, "ORBIT", 4.0)

    sender.send_request("reconfigure", {"voltage": 12})
    time.sleep(0.5)
    req = sender.get_message()
    assert isinstance(req, CSCP1Message)
    assert req.verb_type == CSCP1Message.Type.SUCCESS

    wait_for_state(sat.fsm, "ORBIT", 4.0)
    assert sat.reconfigured


def test_async_satellite_interrupt(mock_async_cmd_transmitter, async_satellite_fixture):
    """Verify interrupt from RUN transitions to SAFE."""
    sender = mock_async_cmd_transmitter
    sat = async_satellite_fixture

    for cmd, payload in [("initialize", {"k": "v"}), ("launch", None), ("start", "5001")]:
        sender.send_request(cmd, payload)
        time.sleep(0.3)
        sender.get_message()

    wait_for_state(sat.fsm, "RUN", 4.0)

    sender.send_request("_interrupt")
    time.sleep(0.3)
    sender.get_message()

    wait_for_state(sat.fsm, "SAFE", 4.0)
    assert sat.fsm.state == SatelliteState.SAFE


def test_async_satellite_run_fail(mock_async_cmd_transmitter, async_fail_satellite_fixture):
    """Verify a satellite that raises in do_run reaches ERROR state."""
    sender = mock_async_cmd_transmitter
    sat = async_fail_satellite_fixture

    for cmd, payload in [("initialize", {"k": "v"}), ("launch", None), ("start", "5001")]:
        sender.send_request(cmd, payload)
        time.sleep(0.3)
        sender.get_message()

    wait_for_state(sat.fsm, "ERROR", 4.0)
    assert sat.fsm.state == SatelliteState.ERROR


def test_async_satellite_shutdown(mock_async_cmd_transmitter, async_satellite_fixture):
    """Verify shutdown command is accepted."""
    sender = mock_async_cmd_transmitter
    _ = async_satellite_fixture  # noqa: F841

    sender.send_request("shutdown")
    time.sleep(0.3)
    req = sender.get_message()
    assert isinstance(req, CSCP1Message)
    assert req.verb_type == CSCP1Message.Type.SUCCESS


def test_async_satellite_invalid_transition(mock_async_cmd_transmitter, async_satellite_fixture):
    """Verify that invalid transitions are rejected."""
    sender = mock_async_cmd_transmitter
    _ = async_satellite_fixture  # noqa: F841

    sender.send_request("launch")
    time.sleep(0.3)
    req = sender.get_message()
    assert isinstance(req, CSCP1Message)
    assert req.verb_type == CSCP1Message.Type.INVALID


def test_async_satellite_unknown_command(mock_async_cmd_transmitter, async_satellite_fixture):
    """Verify that unknown commands return UNKNOWN."""
    sender = mock_async_cmd_transmitter
    _ = async_satellite_fixture  # noqa: F841

    sender.send_request("nonexistent_command")
    time.sleep(0.3)
    req = sender.get_message()
    assert isinstance(req, CSCP1Message)
    assert req.verb_type == CSCP1Message.Type.UNKNOWN
