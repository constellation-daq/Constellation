"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2
"""

import operator
import os
import random
import threading
import time
from tempfile import TemporaryDirectory
from unittest.mock import Mock, patch

import pytest
import zmq

from constellation.core.cdtp import DataTransmitter
from constellation.core.chirp import (
    CHIRP_PORT,
    CHIRPBeaconTransmitter,
)
from constellation.core.configuration import Configuration, flatten_config, load_config
from constellation.core.controller import BaseController
from constellation.core.cscp import CommandTransmitter
from constellation.core.heartbeatchecker import HeartbeatChecker
from constellation.core.logging import setup_cli_logging
from constellation.core.monitoring import FileMonitoringListener
from constellation.core.satellite import Satellite

# chirp
mock_chirp_packet_queue = []

# satellite
mock_packet_queue_recv = {}
mock_packet_queue_sender = {}
send_port = 11111
recv_port = 22222

SNDMORE_MARK = "_S/END_"  # Arbitrary marker for SNDMORE flag used in mocket packet queues_
CHIRP_OFFER_CTRL = b"\x96\xa9CHIRP%x01\x02\xc4\x10\xc3\x941\xda'\x96_K\xa6JU\xac\xbb\xfe\xf1\xac\xc4\x10:\xb9W2E\x01R\xa2\x93|\xddA\x9a%\xb6\x90\x01\xcda\xa9"  # noqa: E501


setup_cli_logging("TRACE")


class chirpsocket:
    """Mocks socket.socket.

    Used in tests involving CHIRP."""

    def __init__(self, *args, **kwargs):
        self.seen: int = 0
        self.timeout: float = -1

    def connected(self):
        return True

    def sendto(self, buf, addr):
        """Append buf to queue."""
        mock_chirp_packet_queue.append(buf)

    def setblocking(self, *args, **kwargs):
        """ignored"""
        pass

    def settimeout(self, timeout: float):
        """Adjust timeout."""
        self.timeout = timeout

    def bind(self, *args, **kwargs):
        """ignored"""
        pass

    def recvfrom(self, bufsize):
        """Get next entry from queue."""
        time.sleep(0.05)
        try:
            data = mock_chirp_packet_queue[self.seen]
            self.seen += 1
            return data, ["127.0.0.1", CHIRP_PORT]
        except IndexError:
            if self.timeout > 0:
                raise TimeoutError("no mock data")
            raise BlockingIOError("no mock data")

    def recvmsg(self, bufsize, ancsize):
        """Get next entry from queue."""
        time.sleep(0.05)
        try:
            # ancillary data for localhost:
            data = mock_chirp_packet_queue[self.seen]
            self.seen += 1
            ancdata = [
                (
                    0,
                    20,
                    b"\x02\x00\x1b\xd3\x7f\x00\x00\xff\x00\x00\x00\x00\x00\x00\x00\x00",
                )
            ]
            return data, ancdata, 0, ["127.0.0.1", CHIRP_PORT]
        except IndexError:
            raise TimeoutError("no mock data")

    def setsockopt(self, *args, **kwargs):
        """Ignored."""
        pass


@pytest.fixture
def mock_chirp_socket():
    """Mock CHIRP socket calls."""
    with patch("constellation.core.multicast.socket.socket") as mock:
        mock.side_effect = chirpsocket
        yield mock


@pytest.fixture
def mock_chirp_transmitter(mock_chirp_socket):
    """Yields a CHIRP transmitter for our fake Constellation."""
    t = CHIRPBeaconTransmitter("mock_transmitter", "mockstellation", "127.0.0.1")
    yield t


class mocket:
    """Mock ZMQ socket for a sender or receiver.

    Select which is the case by setting the endpoint attribute."""

    def __init__(self, *args, **kwargs):
        super().__init__()
        self.port = 0
        # sender/receiver?
        self.endpoint = 0  # 0 or 1

    def _get_queue(self, out: bool):
        """Flip what queue to use depending on direction and endpoint.

        Makes sure that A sends on B's receiving queue and vice-versa.

        """
        if operator.xor(self.endpoint, out):
            return mock_packet_queue_sender
        else:
            return mock_packet_queue_recv

    def send(self, payload, flags=None):
        """Append buf to queue."""
        try:
            if isinstance(flags, zmq.Flag) and zmq.SNDMORE in flags:
                self._get_queue(True)[self.port].append(payload)
            else:
                self._get_queue(True)[self.port].append([payload, SNDMORE_MARK])
        except KeyError:
            if isinstance(flags, zmq.Flag) and zmq.SNDMORE in flags:
                self._get_queue(True)[self.port] = [payload]
            else:
                self._get_queue(True)[self.port] = [[payload, SNDMORE_MARK]]

    def send_string(self, payload, flags=None):
        self.send(payload.encode(), flags=flags)

    def send_multipart(self, msg_parts, flags=None):
        print(f"mock sending {msg_parts} on port {self.port}")
        for idx, msg in enumerate(msg_parts):
            flag = zmq.SNDMORE if idx < len(msg_parts) - 1 else None
            self.send(msg, flag)

    def recv_multipart(self, flags=None):
        """Pop entry from queue."""
        if flags == zmq.NOBLOCK:
            if self.port not in self._get_queue(False) or not self._get_queue(False)[self.port]:
                raise zmq.ZMQError("Resource temporarily unavailable")
        else:
            while self.port not in self._get_queue(False) or not self._get_queue(False)[self.port]:
                time.sleep(0.01)
        r = []
        RCV_MORE = True
        while RCV_MORE:
            dat = self._get_queue(False)[self.port].pop(0)
            if isinstance(dat, list) and SNDMORE_MARK in dat:
                RCV_MORE = False
                r.append(dat[0])
            else:
                r.append(dat)
        return r

    def recv(self, flags=None):
        """Pop single entry from queue."""
        if flags == zmq.NOBLOCK:
            if self.has_no_data():
                raise zmq.ZMQError("Resource temporarily unavailable")

            dat = self._get_queue(False)[self.port].pop(0)

            if isinstance(dat, list) and SNDMORE_MARK in dat:
                r = dat[0]
            else:
                r = dat
            return r
        else:
            # block
            while self.has_no_data():
                time.sleep(0.01)
            dat = self._get_queue(False)[self.port].pop(0)
            if isinstance(dat, list) and SNDMORE_MARK in dat:
                r = dat[0]
            else:
                r = dat
            return r

    def bind(self, host):
        self.port = int(host.split(":")[2])
        print(f"Bound Mocket on {self.port}")

    def bind_to_random_port(self, host):
        self.port = random.randrange(10000, 55555)
        print(f"Bound Mocket on random port: {self.port}")
        return self.port

    def connect(self, host):
        self.port = int(host.split(":")[2])
        print(f"Bound Mocket on {self.port}")

    def has_no_data(self):
        return self.port not in self._get_queue(False) or not self._get_queue(False)[self.port]

    def setsockopt_string(self, *args, **kwargs):
        pass

    def setsockopt(self, *args, **kwargs):
        pass


@pytest.fixture
def mock_socket_sender():
    mock = mocket()
    mock.endpoint = 1
    mock.port = send_port
    yield mock


@pytest.fixture
def mock_socket_receiver():
    mock = mocket()
    mock.endpoint = 0
    mock.port = send_port
    yield mock


@pytest.fixture
def mock_cmd_transmitter(mock_socket_sender):
    t = CommandTransmitter("mock_sender", mock_socket_sender)
    yield t


@pytest.fixture
def mock_data_transmitter(mock_socket_sender):
    t = DataTransmitter("mock_sender", mock_socket_sender)
    yield t


@pytest.fixture
def mock_data_receiver(mock_socket_receiver):
    r = DataTransmitter("mock_receiver", mock_socket_receiver)
    yield r


@pytest.fixture
def mock_heartbeat_checker():
    """Create a mock HeartbeatChecker instance."""

    mockets = []

    def mocket_factory(*args, **kwargs):
        m = mocket()
        m.endpoint = 1
        mockets.append([m, 1])
        return m

    def poll(*args, **kwargs):
        res = [m for m in mockets if not m[0].has_no_data()]
        timeout = 0.05
        while not res and timeout > 0:
            time.sleep(0.01)
            timeout -= 0.01
            res = [m for m in mockets if not m[0].has_no_data()]
        return res

    with patch("constellation.core.heartbeatchecker.zmq.Context") as mock:
        with patch("constellation.core.heartbeatchecker.zmq.Poller") as mock_p:
            mock_context = Mock()
            mock_context.socket = mocket_factory
            mock.return_value = mock_context
            mock_poller = Mock()
            mock_poller.poll.side_effect = poll
            mock_p.return_value = mock_poller
            hbc = HeartbeatChecker("mock_hbchecker", "127.0.0.1")
            hbc._add_com_thread()
            hbc._start_com_threads()
            # give the threads a chance to start
            time.sleep(0.1)
            yield hbc


@pytest.fixture
def mock_satellite(mock_chirp_transmitter, mock_heartbeat_checker):
    """Create a mock Satellite base instance."""

    def mocket_factory(*args, **kwargs):
        m = mocket()
        return m

    with patch("constellation.core.base.zmq.Context") as mock:
        mock_context = Mock()
        mock_context.socket = mocket_factory
        mock.return_value = mock_context
        s = Satellite("mock_satellite", "mockstellation", 11111, 22222, 33333, "127.0.0.1")
        t = threading.Thread(target=s.run_satellite)
        t.start()
        # give the threads a chance to start
        time.sleep(0.1)
        yield s


@pytest.fixture
def mock_controller(mock_chirp_transmitter, mock_heartbeat_checker):
    """Create a mock Controller base instance."""

    def mocket_factory(*args, **kwargs):
        m = mocket()
        m.endpoint = 1
        return m

    with patch("constellation.core.base.zmq.Context") as mock:
        mock_context = Mock()
        mock_context.socket = mocket_factory
        mock.return_value = mock_context
        c = BaseController(name="mock_controller", group="mockstellation", interface="127.0.0.1")
        # give the threads a chance to start
        time.sleep(0.1)
        yield c


@pytest.fixture
def rawconfig():
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "test_cfg.toml")
    yield load_config(path)


@pytest.fixture
def config(rawconfig):
    """Fixture for specific configuration"""
    config = flatten_config(
        rawconfig,
        "mocksat",
        "device2",
    )
    yield Configuration(config)


@pytest.fixture
def mock_example_satellite(mock_chirp_transmitter):
    """Mock a Satellite for a specific device, ie. a class inheriting from Satellite."""

    def mocket_factory(*args, **kwargs):
        m = mocket()
        return m

    class MockExampleSatellite(Satellite):
        def do_initializing(self, payload):
            self.voltage = self.config.setdefault("voltage", 10)
            self.mode = self.config.setdefault("mode", "passionate")
            return "finished with mock initialization"

    with patch("constellation.core.base.zmq.Context") as mock:
        mock_context = Mock()
        mock_context.socket = mocket_factory
        mock.return_value = mock_context
        s = MockExampleSatellite("mock_satellite", "mockstellation", 11111, 22222, 33333, "127.0.0.1")
        t = threading.Thread(target=s.run_satellite)
        t.start()
        # give the threads a chance to start
        time.sleep(0.1)
        yield s


@pytest.fixture
def monitoringlistener():
    """Create a MonitoringListener instance."""

    with TemporaryDirectory(prefix="constellation_pytest_") as tmpdirname:
        m = FileMonitoringListener(
            name="mock_monitor",
            group="mockstellation",
            interface="*",
            output_path=tmpdirname,
        )
        t = threading.Thread(target=m.run_listener)
        t.start()
        # give the thread a chance to start
        time.sleep(0.1)
        yield m, tmpdirname


def wait_for_state(fsm, state: str, timeout: float = 2.0):
    while timeout > 0 and fsm.current_state_value.name != state:
        time.sleep(0.05)
        timeout -= 0.05
    if timeout < 0:
        raise RuntimeError(f"Never reached {state}, now in state {fsm.current_state_value.name} with status '{fsm.status}'")
