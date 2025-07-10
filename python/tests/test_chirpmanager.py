"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2
"""

import time
from unittest.mock import MagicMock

import pytest

from constellation.core.chirp import CHIRPServiceIdentifier
from constellation.core.chirpmanager import CHIRPManager, chirp_callback
from constellation.core.network import get_loopback_interface_name

offer_data_666 = b"CHIRP\x01\x02\xd4fl\x89\x14g7=*b#\xeb4fy\xda\x17\x7f\xd1\xa7t\xc5\xb6/\xd5\xcc$e\x01\x81ir\x04\x02\x9a"

# SIDE EFFECTS


# FIXTURES
@pytest.fixture
def mock_bm(mock_chirp_socket):
    """Create mock CHIRPManager."""
    bm = CHIRPManager(name="mock_satellite", group="mockstellation", interface=[get_loopback_interface_name()])
    bm._add_com_thread()
    bm._start_com_threads()
    yield bm, mock_chirp_socket
    bm.reentry()
    bm.context.destroy()


@pytest.fixture
def mock_bm_parent(mock_chirp_socket):
    """Create mock class inheriting from CHIRPManager."""

    class MockCHIRPManager(CHIRPManager):
        callback_triggered = False

        @chirp_callback(CHIRPServiceIdentifier.DATA)
        def service_callback(self, service):
            self.callback_triggered = True

    bm = MockCHIRPManager(name="mock_satellite", group="mockstellation", interface=[get_loopback_interface_name()])
    bm._add_com_thread()
    bm._start_com_threads()
    yield bm, mock_chirp_socket
    bm.reentry()
    bm.context.destroy()


@pytest.fixture
def mock_bm_alt_parent(mock_chirp_socket):
    """Create alternative mock class inheriting from CHIRPManager.

    Does not use callback decorator.

    """

    class MockAltCHIRPManager(CHIRPManager):
        alt_callback_triggered = False

        def alt_service_callback(self, service):
            self.alt_callback_triggered = True

    bm = MockAltCHIRPManager(name="mock_satellite", group="mockstellation", interface=[get_loopback_interface_name()])
    bm._add_com_thread()
    bm._start_com_threads()
    yield bm, mock_chirp_socket
    bm.reentry()
    bm.context.destroy()


def test_manager_register(mock_bm):
    """Test registering services."""
    bm, sock = mock_bm
    bm.register_offer(CHIRPServiceIdentifier.HEARTBEAT, 50000)
    bm.register_offer(CHIRPServiceIdentifier.CONTROL, 50001)
    bm.emit_offers()
    assert len(sock._packet_queue) == 2

    # Offer only one of the services
    bm.emit_offers(CHIRPServiceIdentifier.CONTROL)
    assert len(sock._packet_queue) == 3


def test_manager_discover(mock_bm):
    """Test discovering services."""
    bm, sock = mock_bm
    sock._packet_queue.append(offer_data_666)
    # thread running in background listening to "socket"
    time.sleep(0.5)
    bm._stop_com_threads()
    assert bm._beacon._socket._recv_socket.seen == 1
    assert len(bm.discovered_services) == 1
    # check late callback queuing
    mock = MagicMock()
    assert bm.task_queue.empty()
    bm.register_request(CHIRPServiceIdentifier.DATA, mock.callback)
    assert not bm.task_queue.empty()
    # no actual callback happens (only queued)
    assert mock.callback.call_count == 0


def test_manager_ext_callback_runtime(mock_bm):
    """Test callback when discovering services registered during runtime."""
    bm, sock = mock_bm
    # create external callback
    global callback_seen
    callback_seen = False

    def mycallback(service):
        global callback_seen
        callback_seen = True

    assert bm.task_queue.empty()
    bm.register_request(CHIRPServiceIdentifier.DATA, mycallback)
    sock._packet_queue.append(offer_data_666)
    # thread running in background listening to "socket"
    time.sleep(0.5)
    assert bm._beacon._socket._recv_socket.seen == 1
    assert len(bm.discovered_services) == 1
    # callback queued but not performed (no worker thread)
    assert not bm.task_queue.empty()
    assert not callback_seen
    fcn, arg = bm.task_queue.get()
    fcn(*arg)
    assert callback_seen


def test_manager_method_callback_runtime(mock_bm_alt_parent):
    """Test callback when discovering services registered during runtime."""
    bm, sock = mock_bm_alt_parent
    assert bm.task_queue.empty()
    bm.register_request(
        CHIRPServiceIdentifier.DATA,
        bm.alt_service_callback,
    )
    sock._packet_queue.append(offer_data_666)
    # thread running in background listening to "socket"
    time.sleep(0.5)
    assert bm._beacon._socket._recv_socket.seen == 1
    assert len(bm.discovered_services) == 1
    # callback queued but not performed (no worker thread)
    assert not bm.alt_callback_triggered
    assert not bm.task_queue.empty()
    fcn, arg = bm.task_queue.get()
    fcn(*arg)
    assert bm.alt_callback_triggered


def test_manager_callback_decorator(mock_bm_parent):
    """Test callback when discovering services registered via decorator."""
    bm, sock = mock_bm_parent
    # create callback
    assert bm.task_queue.empty()
    assert len(bm._chirp_callbacks) == 1
    sock._packet_queue.append(offer_data_666)
    # thread running in background listening to "socket"
    time.sleep(0.5)
    assert bm._beacon._socket._recv_socket.seen == 1
    assert len(bm.discovered_services) == 1
    # callback queued but not performed (no worker thread)
    assert not bm.task_queue.empty()
    assert not bm.callback_triggered
    fcn, arg = bm.task_queue.get()
    fcn(*arg)
    assert bm.callback_triggered
