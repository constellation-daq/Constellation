"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0
"""

import pytest
from unittest.mock import patch, MagicMock
import operator
import time
import zmq

from constellation.chirp import (
    CHIRP_PORT,
    CHIRPBeaconTransmitter,
)

from constellation.cscp import CommandTransmitter

# chirp
mock_chirp_packet_queue = []

# satellite
mock_packet_queue_recv = {}
mock_packet_queue_sender = {}
send_port = 11111


# SIDE EFFECTS
def mock_chirp_sock_sendto(buf, addr):
    """Append buf to queue."""
    mock_chirp_packet_queue.append(buf)


def mock_chirp_sock_recvfrom(bufsize):
    """Pop entry from queue."""
    try:
        return mock_chirp_packet_queue.pop(0), ["somehost", CHIRP_PORT]
    except IndexError:
        raise BlockingIOError("no mock data")


@pytest.fixture
def mock_chirp_socket():
    with patch("constellation.chirp.socket.socket") as mock:
        mock = mock.return_value
        mock.connected = MagicMock(return_value=True)
        mock.sendto = MagicMock(side_effect=mock_chirp_sock_sendto)
        mock.recvfrom = MagicMock(side_effect=mock_chirp_sock_recvfrom)
        yield mock


@pytest.fixture
def mock_chirp_transmitter(mock_chirp_socket):
    t = CHIRPBeaconTransmitter(
        "mock_sender",
        "mockstellation",
    )
    yield t


class mocket(MagicMock):
    """Mock socket for a receiver."""

    def __init__(self, *args, **kwargs):
        super().__init__()
        self.port = 0
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
            self._get_queue(True)[self.port].append(payload)
        except KeyError:
            self._get_queue(True)[self.port] = [payload]

    def send_string(self, payload, flags=None):
        self.send(payload.encode())

    def recv_multipart(self, flags=None):
        """Pop entry from queue."""
        if (
            self.port not in self._get_queue(False)
            or not self._get_queue(False)[self.port]
        ):
            raise zmq.ZMQError("no mock data")
        # "pop all"
        r, self._get_queue(False)[self.port][:] = (
            self._get_queue(False)[self.port][:],
            [],
        )
        return r

    def recv(self, flags=None):
        """Pop single entry from queue."""
        if flags == zmq.NOBLOCK:
            if (
                send_port not in self._get_queue(False)
                or not self._get_queue(False)[send_port]
            ):
                raise zmq.ZMQError("no mock data")
            return self._get_queue(False)[send_port].pop(0)
        else:
            # block
            while (
                send_port not in self._get_queue(False)
                or not self._get_queue(False)[send_port]
            ):
                time.sleep(0.01)
            return self._get_queue(False)[send_port].pop(0)

    def bind(self, host):
        self.port = int(host.split(":")[2])
        print(f"Bound Mocket on {self.port}")

    def connect(self, host):
        self.port = int(host.split(":")[2])
        print(f"Bound Mocket on {self.port}")


@pytest.fixture
def mock_socket_sender():
    mock = mocket()
    mock.return_value = mock
    mock.endpoint = 1
    mock.port = send_port
    yield mock


@pytest.fixture
def mock_cmd_transmitter(mock_socket_sender):
    t = CommandTransmitter("mock_sender", mock_socket_sender)
    yield t
