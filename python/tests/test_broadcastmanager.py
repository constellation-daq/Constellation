#!/usr/bin/env python3
"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0
"""

import pytest
import time
from unittest.mock import patch, MagicMock

from constellation.broadcastmanager import CHIRPBroadcaster, chirp_callback, CALLBACKS

from constellation.chirp import CHIRPServiceIdentifier, CHIRP_PORT

offer_data_666 = b"\x96\xa9CHIRP%x01\x02\xc4\x10\xe8\x05\x01\xfc\x10\x00Z\xfa\x8c\xfa\x96\x8c\xa4\xb5\x13j\xc4\x10,\x13>\x81\xd4&Z2\xa3\xae\xa6\x9c\xc5aS\x82\x04\xcd\x02\x9a"  # noqa: E501


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
    bm = CHIRPBroadcaster(
        name="mock-satellite",
        group="mockstellation",
    )
    bm._add_com_thread()
    bm._start_com_threads()
    yield bm


@pytest.fixture
def mock_bm_parent(mock_socket):
    """Create mock class inheriting from BroadcastManager."""

    class MockBroadcaster(CHIRPBroadcaster):
        callback_triggered = False

        @chirp_callback(CHIRPServiceIdentifier.DATA)
        def service_callback(self, service):
            self.callback_triggered = True

    bm = MockBroadcaster(
        name="mock-satellite",
        group="mockstellation",
    )
    bm._add_com_thread()
    bm._start_com_threads()
    yield bm


@pytest.fixture
def mock_bm_alt_parent(mock_socket):
    """Create alternative mock class inheriting from BroadcastManager.

    Does not use callback decorator.

    """

    class MockAltBroadcaster(CHIRPBroadcaster):
        alt_callback_triggered = False

        def alt_service_callback(self, service):
            self.alt_callback_triggered = True

    bm = MockAltBroadcaster(
        name="mock-satellite",
        group="mockstellation",
    )
    bm._add_com_thread()
    bm._start_com_threads()
    yield bm


@pytest.mark.forked
def test_manager_register(mock_bm):
    """Test registering services."""
    mock_bm.register_offer(CHIRPServiceIdentifier.HEARTBEAT, 50000)
    mock_bm.register_offer(CHIRPServiceIdentifier.CONTROL, 50001)
    mock_bm.broadcast_offers()
    assert len(mock_packet_queue) == 2

    # broadcast only one of the services
    mock_bm.broadcast_offers(CHIRPServiceIdentifier.CONTROL)
    assert len(mock_packet_queue) == 3


@pytest.mark.forked
def test_manager_discover(mock_bm):
    """Test discovering services."""
    mock_packet_queue.append(offer_data_666)
    # thread running in background listening to "socket"
    time.sleep(0.5)
    mock_bm._stop_com_threads()
    assert len(mock_packet_queue) == 0
    assert len(mock_bm.discovered_services) == 1
    # check late callback queuing
    mock = MagicMock()
    assert mock_bm.task_queue.empty()
    mock_bm.register_request(CHIRPServiceIdentifier.DATA, mock.callback)
    assert not mock_bm.task_queue.empty()
    # no actual callback happens (only queued)
    assert mock.callback.call_count == 0


@pytest.mark.forked
def test_manager_ext_callback_runtime(mock_bm):
    """Test callback when discovering services registered during rumtime."""
    # create external callback
    mock = MagicMock()
    assert mock_bm.task_queue.empty()
    mock_bm.register_request(CHIRPServiceIdentifier.DATA, mock.callback)
    mock_packet_queue.append(offer_data_666)
    # thread running in background listening to "socket"
    time.sleep(0.5)
    assert len(mock_packet_queue) == 0
    assert len(mock_bm.discovered_services) == 1
    # callback queued but not performed (no worker thread)
    assert not mock_bm.task_queue.empty()
    assert mock.callback.call_count == 0
    fcn, arg = mock_bm.task_queue.get()
    fcn(*arg)
    assert mock.callback.call_count == 1


@pytest.mark.forked
def test_manager_method_callback_runtime(mock_bm_alt_parent):
    """Test callback when discovering services registered during rumtime."""
    assert mock_bm_alt_parent.task_queue.empty()
    mock_bm_alt_parent.register_request(
        CHIRPServiceIdentifier.DATA,
        (lambda _self, service: mock_bm_alt_parent.alt_service_callback(service)),
    )
    mock_packet_queue.append(offer_data_666)
    # thread running in background listening to "socket"
    time.sleep(0.5)
    assert len(mock_packet_queue) == 0
    assert len(mock_bm_alt_parent.discovered_services) == 1
    # callback queued but not performed (no worker thread)
    assert not mock_bm_alt_parent.alt_callback_triggered
    assert not mock_bm_alt_parent.task_queue.empty()
    fcn, arg = mock_bm_alt_parent.task_queue.get()
    fcn(*arg)
    assert mock_bm_alt_parent.alt_callback_triggered


@pytest.mark.forked
def test_manager_callback_decorator(mock_bm_parent):
    """Test callback when discovering services registered via decorator."""
    # create callback
    assert mock_bm_parent.task_queue.empty()
    assert len(CALLBACKS) == 1
    mock_packet_queue.append(offer_data_666)
    # thread running in background listening to "socket"
    time.sleep(0.5)
    assert len(mock_packet_queue) == 0
    assert len(mock_bm_parent.discovered_services) == 1
    # callback queued but not performed (no worker thread)
    assert not mock_bm_parent.task_queue.empty()
    assert not mock_bm_parent.callback_triggered
    fcn, arg = mock_bm_parent.task_queue.get()
    fcn(*arg)
    assert mock_bm_parent.callback_triggered
