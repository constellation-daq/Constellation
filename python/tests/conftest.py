"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0
"""

import pytest
from unittest.mock import patch, MagicMock

from constellation.chirp import (
    CHIRP_PORT,
    CHIRPBeaconTransmitter,
)

mock_chirp_packet_queue = []


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
