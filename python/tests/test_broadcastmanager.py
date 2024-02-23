#!/usr/bin/env python3
"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0
"""

import pytest
from queue import Queue
import time
from unittest.mock import patch, MagicMock

from constellation.broadcastmanager import (
    CHIRPBroadcastManager,
)

from constellation.chirp import CHIRPServiceIdentifier, CHIRP_PORT

offer_data_666 = b"\x96\xa9CHIRP%x01\x02\xc4\x10\xe8\x05\x01\xfc\x10\x00Z\xfa\x8c\xfa\x96\x8c\xa4\xb5\x13j\xc4\x10,\x13>\x81\xd4&Z2\xa3\xae\xa6\x9c\xc5aS\x82\x04\xcd\x02\x9a"


mock_packet_queue = []


# SIDE EFFECTS
def mock_sock_sendto(buf, addr):
    """Append buf to response list."""
    mock_packet_queue.append(buf)


def mock_sock_recvfrom(bufsize):
    """Pop entry from response list."""
    try:
        return mock_packet_queue.pop(0), ["somehost", CHIRP_PORT]
    except IndexError:
        raise BlockingIOError("no mock data")


# FIXTURES
@pytest.fixture
def mock_socket():
    """Mock socket calls."""
    with patch("constellation.chirp.socket.socket") as mock:
        mock = mock.return_value
        mock.connected = MagicMock(return_value=True)
        mock.sendto = MagicMock(side_effect=mock_sock_sendto)
        mock.recvfrom = MagicMock(side_effect=mock_sock_recvfrom)
        yield mock


@pytest.fixture
def mock_bm(mock_socket):
    """Create mock BroadcastManager."""
    q = Queue()
    bm = CHIRPBroadcastManager(
        name="mock-satellite",
        group="mockstellation",
        callback_queue=q,
    )
    yield bm, q


def test_manager_register(mock_bm):
    """Test registering services."""
    bm, q = mock_bm

    bm.register_offer(CHIRPServiceIdentifier.HEARTBEAT, 50000)
    bm.register_offer(CHIRPServiceIdentifier.CONTROL, 50001)
    bm.broadcast_offers()
    assert len(mock_packet_queue) == 2

    # broadcast only one of the services
    bm.broadcast_offers(CHIRPServiceIdentifier.CONTROL)
    assert len(mock_packet_queue) == 3


def test_manager_discover(mock_bm):
    """Test discovering services."""
    bm, q = mock_bm

    bm.start()
    mock_packet_queue.append(offer_data_666)
    # thread running in background listening to "socket"
    time.sleep(0.5)
    bm.stop()
    assert len(mock_packet_queue) == 0
    assert len(bm.discovered_services) == 1
    # check late callback queuing
    mock = MagicMock()
    assert q.empty()
    bm.register_request(CHIRPServiceIdentifier.DATA, mock.callback)
    assert not q.empty()
    # no actual callback happens (only queued)
    assert mock.callback.call_count == 0
