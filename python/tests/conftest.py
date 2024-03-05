"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0
"""

import pytest
from unittest.mock import patch, MagicMock
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
        print(f"Bound Mocket on {self.port}")


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
def mock_cmd_transmitter(mock_socket_sender):
    t = CommandTransmitter("mock_sender", mock_socket_sender)
    yield t
