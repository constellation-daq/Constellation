#!/usr/bin/env python3
"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0
"""

import pytest

from unittest.mock import patch, MagicMock

from constellation.chirp import (
    CHIRPBeaconTransmitter,
    CHIRPServiceIdentifier,
    CHIRPMessageType,
    CHIRP_PORT,
)

mock_packet_queue = []


# SIDE EFFECTS
def mock_sock_sendto(buf, addr):
    """Append buf to queue."""
    mock_packet_queue.append(buf)


def mock_sock_recvfrom(bufsize):
    """Pop entry from queue."""
    try:
        return mock_packet_queue.pop(0), ["somehost", CHIRP_PORT]
    except IndexError:
        raise BlockingIOError("no mock data")


# FIXTURES
@pytest.fixture
def mock_socket():
    with patch("constellation.chirp.socket.socket") as mock:
        mock = mock.return_value
        mock.connected = MagicMock(return_value=True)
        mock.sendto = MagicMock(side_effect=mock_sock_sendto)
        mock.recvfrom = MagicMock(side_effect=mock_sock_recvfrom)
        yield mock


@pytest.fixture
def mock_transmitter(mock_socket):
    t = CHIRPBeaconTransmitter(
        "mock_sender",
        "mockstellation",
    )
    yield t


def test_chirp_beacon_send_recv(mock_socket):
    """Test interplay between two transmitters (sender/receiver)."""
    sender = CHIRPBeaconTransmitter(
        "mock_sender",
        "mockstellation",
    )
    receiver = CHIRPBeaconTransmitter(
        "mock_receiver",
        "mockstellation",
    )
    sender.broadcast(CHIRPServiceIdentifier.DATA, CHIRPMessageType.OFFER, 666)
    res = receiver.listen()
    assert (
        res.serviceid == CHIRPServiceIdentifier.DATA
    ), "Receiving chirp package failed."

    # listen a second time
    res = receiver.listen()
    assert not res, "Non-blocking receive w/o sending data failed."

    # receive two packages
    sender.broadcast(CHIRPServiceIdentifier.DATA, CHIRPMessageType.OFFER, 666)
    sender.broadcast(CHIRPServiceIdentifier.DATA, CHIRPMessageType.OFFER, 666)
    res = receiver.listen()
    assert (
        res.serviceid == CHIRPServiceIdentifier.DATA
    ), "First of two packages went missing"
    res = receiver.listen()
    assert (
        res.serviceid == CHIRPServiceIdentifier.DATA
    ), "Second of two packages went missing"
    res = receiver.listen()
    assert not res, "Non-blocking receive w/o sending data failed."

    # send package, then let the test for header fail
    sender.broadcast(CHIRPServiceIdentifier.DATA, CHIRPMessageType.OFFER, 666)
    with patch("constellation.chirp.CHIRP_HEADER"):
        with pytest.raises(RuntimeError) as e:
            res = receiver.listen()
        assert "malformed CHIRP header" in str(
            e.value
        ), "Wrong chirp header did not trigger expected exception message."


def test_filter_same_host(mock_transmitter):
    """Check that same-host packets are dropped."""
    t = mock_transmitter
    t.broadcast(CHIRPServiceIdentifier.DATA, CHIRPMessageType.OFFER, 666)
    res = t.listen()
    assert not res, "Received packet despite same-host filter."