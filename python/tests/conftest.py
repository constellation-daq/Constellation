"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2
"""

import os
import random
import threading
import time
from functools import partial
from tempfile import TemporaryDirectory
from unittest.mock import Mock, patch

import pytest
import zmq

from constellation.core.chirp import CHIRP_PORT, CHIRPBeaconTransmitter
from constellation.core.configuration import Configuration, flatten_config, load_config
from constellation.core.controller import BaseController
from constellation.core.cscp import CommandTransmitter
from constellation.core.heartbeatchecker import HeartbeatChecker
from constellation.core.monitoring import FileMonitoringListener
from constellation.core.network import get_loopback_interface_name
from constellation.core.satellite import Satellite

# a default port to send arbitrary data on
DEFAULT_SEND_PORT = 11111
# port for CDTP
DATA_PORT = 50101
MON_PORT = 33333

SNDMORE_MARK = "_S/END_"  # Arbitrary marker for SNDMORE flag used in mocket packet queues_
CHIRP_OFFER_CTRL = b"\x96\xa9CHIRP%x01\x02\xc4\x10\xc3\x941\xda'\x96_K\xa6JU\xac\xbb\xfe\xf1\xac\xc4\x10:\xb9W2E\x01R\xa2\x93|\xddA\x9a%\xb6\x90\x01\xcda\xa9"  # noqa: E501


class chirpsocket:
    """Mocks socket.socket.

    Used in tests involving CHIRP."""

    def __init__(self, *args, **kwargs):
        self.seen: int = 0
        self._connected = False
        self.timeout: float = -1
        self.mock_chirp_packet_queue = []

    def connected(self):
        return self._connected

    def close(self):
        self._connected = False

    def sendto(self, buf, addr):
        """Append buf to queue."""
        self.mock_chirp_packet_queue.append(buf)

    def setblocking(self, *args, **kwargs):
        """ignored"""
        pass

    def settimeout(self, timeout: float):
        """Adjust timeout."""
        self.timeout = timeout

    def bind(self, *args, **kwargs):
        """ignored"""
        self._connected = True

    def recvfrom(self, bufsize):
        """Get next entry from queue."""
        time.sleep(0.05)
        try:
            data = self.mock_chirp_packet_queue[self.seen]
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
            data = self.mock_chirp_packet_queue[self.seen]
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
    """Mock CHIRP socket calls.

    Creates a packet queue that is shared for all subsequent opened sockets.

    """
    with patch("constellation.core.multicast.socket.socket") as mock:

        # one packet queue per fixture
        packet_queue = []
        mock._packet_queue = packet_queue

        def add_socket_info(mock, *args, **kwargs):
            m = chirpsocket()
            # one packet queue per fixture
            m.mock_chirp_packet_queue = packet_queue
            # keep track of instantiated mock sockets
            if not isinstance(mock._known_sockets, list):
                mock._known_sockets = []
            mock._known_sockets.append(m)
            return m

        mock.side_effect = partial(add_socket_info, mock)
        yield mock


@pytest.fixture
def mock_chirp_transmitter(mock_chirp_socket):
    """Yields a CHIRP transmitter for our fake Constellation."""
    t = CHIRPBeaconTransmitter("mock_transmitter", "mockstellation", ["127.0.0.1"])
    yield t


class mocket:
    """Mock ZMQ socket for a sender or receiver.

    Select which is the case by setting the endpoint attribute."""

    def __init__(self, *args, **kwargs):
        super().__init__()
        self.port = 0
        self.packet_queue_in = {}
        self.packet_queue_out = {}

    def _get_queue(self, out: bool):
        """Return queue depending on direction (outward/inward)."""
        if out:
            return self.packet_queue_out
        return self.packet_queue_in

    def _create_queue(self):
        self._get_queue(True)[self.port] = []
        self._get_queue(False)[self.port] = []

    def _flip_queues(self):
        tmp = self.packet_queue_in
        self.packet_queue_in = self.packet_queue_out
        self.packet_queue_out = tmp

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
        self._create_queue()
        print(f"Bound Mocket on {self.port}")

    def bind_to_random_port(self, host):
        self.port = random.randrange(10000, 55555)
        self._create_queue()
        print(f"Bound Mocket on random port: {self.port}")
        return self.port

    def connect(self, host):
        self.port = int(host.split(":")[2])
        self._create_queue()
        print(f"Bound Mocket on {self.port}")

    def has_no_data(self):
        return self.port not in self._get_queue(False) or not self._get_queue(False)[self.port]

    def setsockopt_string(self, option, value, *args, **kwargs):
        if option == zmq.SUBSCRIBE:
            print(f"subscribing to {value}")
            self.send_multipart([b"\x01" + value.encode()])
        if option == zmq.UNSUBSCRIBE:
            self.send_multipart([b"\x00" + value.encode()])

    def setsockopt(self, *args, **kwargs):
        pass

    def close(self, *args, **kwargs):
        pass


@pytest.fixture
def mock_zmq_context():
    """A mock ZMQ Context factory fixture that creates mock ZMQ sockets.

    All sockets share the same bidirectional packet queue (two dicts). Use the
    `flip_queue` function to exchange the direction.

    For example,

    ```
    ctx = mock_zmq_context()
    ctx.flip_queues()
    ```

    """
    packet_queue_in = {}
    packet_queue_out = {}

    def mocket_factory(ctx, *args, **kwargs):
        m = mocket()
        m.packet_queue_in = ctx.packet_queue_in
        m.packet_queue_out = ctx.packet_queue_out
        # keep track of instantiated mock sockets
        ctx._known_sockets.append(m)
        return m

    def flip_queues(ctx):
        tmp = ctx.packet_queue_in
        ctx.packet_queue_in = ctx.packet_queue_out
        ctx.packet_queue_out = tmp
        ctx.queues_flipped = True

    with patch("constellation.core.heartbeatchecker.zmq.Context") as hbcontext:
        with patch("constellation.core.base.zmq.Context") as basecontext:
            with patch("constellation.core.commandmanager.zmq.Context") as cmdcontext:

                def context_factory():
                    ctx = Mock()
                    # mock context instantiation
                    hbcontext.return_value = ctx
                    basecontext.return_value = ctx
                    cmdcontext.return_value = ctx
                    # ensure that the contexts are not picked up as cscp commands
                    hbcontext.cscp_command = False
                    basecontext.cscp_command = False
                    cmdcontext.cscp_command = False
                    ctx.cscp_command = False
                    # save a reference to the packet queues
                    ctx.queues_flipped = False
                    ctx.packet_queue_in = packet_queue_in
                    ctx.packet_queue_out = packet_queue_out
                    ctx.flip_queues.side_effect = partial(flip_queues, ctx)
                    # return mockets for sockets
                    ctx.endpoint = 0
                    ctx._known_sockets = []
                    ctx.socket = partial(mocket_factory, ctx)
                    return ctx

            yield context_factory


@pytest.fixture
def mock_socket_sender(mock_zmq_context):
    ctx = mock_zmq_context()
    socket = ctx.socket()
    socket.port = DEFAULT_SEND_PORT
    yield socket


@pytest.fixture
def mock_socket_receiver(mock_zmq_context):
    ctx = mock_zmq_context()
    ctx.flip_queues()
    socket = ctx.socket()
    socket.port = DEFAULT_SEND_PORT
    yield socket


@pytest.fixture
def mock_cmd_transmitter(mock_socket_sender):
    t = CommandTransmitter("mock_sender", mock_socket_sender)
    yield t
    t.close()


@pytest.fixture
def mock_poller(mock_zmq_context):
    """Create a mock poller."""

    with patch("constellation.core.heartbeatchecker.zmq.Poller") as hbcpoller:
        with patch("constellation.core.cdtp.zmq.Poller") as drcpoller:
            ctx = mock_zmq_context()
            ctx.flip_queues()  # flip bidirectional queues
            mockets = ctx._known_sockets

            def poll(*args, **kwargs):
                """Poll known sockets mimicking a ZMQ Poller."""
                res = [[m, 1] for m in mockets if not m.has_no_data()]
                timeout = 0.05
                while not res and timeout > 0:
                    time.sleep(0.01)
                    timeout -= 0.01
                    res = [[m, 1] for m in mockets if not m.has_no_data()]
                return res

            mock_poller = Mock()
            mock_poller.poll.side_effect = poll
            mock_poller.cscp_command = False
            hbcpoller.return_value = mock_poller
            drcpoller.return_value = mock_poller
            yield ctx, mock_poller


@pytest.fixture
def mock_heartbeat_checker(mock_poller):
    """Create a mock HeartbeatChecker instance."""
    ctx, poller = mock_poller
    hbc = HeartbeatChecker("mock_hbchecker")
    hbc._add_com_thread()
    hbc._start_com_threads()
    # give the threads a chance to start
    time.sleep(0.1)
    yield hbc, ctx
    # teardown
    hbc.reentry()


@pytest.fixture
def mock_satellite(mock_zmq_context, mock_chirp_socket):
    """Create a mock Satellite base instance."""
    ctx = mock_zmq_context()
    ctx.flip_queues()

    s = Satellite("mock_satellite", "mockstellation", 11111, 22222, MON_PORT, [get_loopback_interface_name()])
    t = threading.Thread(target=s.run_satellite)
    t.start()
    # give the threads a chance to start
    time.sleep(0.1)
    yield s, ctx
    # teardown
    s.reentry()


@pytest.fixture
def mock_controller(mock_zmq_context, mock_chirp_socket, mock_poller):
    """Create a mock Controller base instance."""
    ctx = mock_zmq_context()

    c = BaseController(name="mock_controller", group="mockstellation", interface=[get_loopback_interface_name()])
    # give the threads a chance to start
    time.sleep(0.1)
    yield c, ctx
    # teardown
    c.reentry()


@pytest.fixture
def controller():
    """Create a Controller base instance."""
    c = BaseController(name="test_controller", group="mockstellation", interface=[get_loopback_interface_name()])
    # give the threads a chance to start
    time.sleep(0.1)
    yield c
    # teardown
    c.reentry()


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
def mock_example_satellite(mock_zmq_context, mock_chirp_socket):
    """Mock a Satellite for a specific device, ie. a class inheriting from Satellite."""
    ctx = mock_zmq_context()
    ctx.flip_queues()

    class MockExampleSatellite(Satellite):
        def do_initializing(self, payload):
            self.voltage = self.config.setdefault("voltage", 10)
            self.mode = self.config.setdefault("mode", "passionate")
            return "finished with mock initialization"

        def do_reconfigure(self, payload):
            self.voltage = self.config.setdefault("voltage", 100)
            self.mode = self.config.setdefault("mode", "cautious")
            return "finished with mock reconfiguration"

    s = MockExampleSatellite("mock_satellite", "mockstellation", 11111, 22222, MON_PORT, [get_loopback_interface_name()])
    t = threading.Thread(target=s.run_satellite)
    t.start()
    # give the threads a chance to start
    time.sleep(0.1)
    yield s, ctx
    # teardown
    s.reentry()


@pytest.fixture
def monitoringlistener(request):
    """Create a MonitoringListener instance."""
    marker = request.node.get_closest_marker("constellation")
    if not marker:
        constellation = "mockstellation"
    else:
        constellation = marker.args[0]
    with TemporaryDirectory(prefix="constellation_pytest_") as tmpdirname:
        m = FileMonitoringListener(
            name="mock_monitor",
            group=constellation,
            interface=[get_loopback_interface_name()],
            output_path=tmpdirname,
        )
        t = threading.Thread(target=m.run_listener)
        t.start()
        # give the thread a chance to start
        time.sleep(0.1)
        yield m, tmpdirname
        m.reentry()


def wait_for_state(fsm, state: str, timeout: float = 2.0):
    while timeout > 0 and fsm.current_state_value.name != state:
        time.sleep(0.005)
        timeout -= 0.005
    if timeout < 0:
        raise RuntimeError(f"Never reached {state}, now in state {fsm.current_state_value.name} with status '{fsm.status}'")


def check_output(capsys, caplog) -> None:
    """Function to ensure that there were no error messages printed in the stdout or logs.

    Expects the fixtures capsys and caplog being passed to it.

    """
    captured = capsys.readouterr()
    assert not captured.err, "Error messages were produced, please check printed output of test."
    for record in caplog.records:
        assert record.levelname != "CRITICAL", "Critical error messages were logged"
        assert record.levelname != "ERROR", "Error messages were logged"
