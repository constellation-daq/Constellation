"""
SPDX-FileCopyrightText: 2025 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2

Module implementing receiver pools for ZeroMQ
"""

import os
import threading
from collections.abc import Callable
from uuid import UUID

import zmq

from .chirp import get_uuid


class BasePool:
    """Base pool for ZeroMQ polling"""

    def __init__(self, context: zmq.Context, socket_type: zmq.SocketType, receive_cb: Callable[[list[bytes]], None]):
        self._context = context
        self._socket_type = socket_type
        self._receive_cb = receive_cb

        self._poller = zmq.Poller()
        self._poller_lock = threading.Lock()
        self._sockets: dict[UUID, zmq.Socket] = {}

        self._stopevt = threading.Event()
        self._poll_thread: threading.Thread | None = None
        self._poll_thread_exc: Exception | None = None
        self._polling = False

    @property
    def polling(self) -> bool:
        """If the poller is currently polling"""
        return self._polling

    def start_poll_thread(self) -> None:
        """Start poll thread (if not already started)"""
        if self._poll_thread is None or not self._poll_thread.is_alive():
            self._stopevt.clear()
            self._reset_sockets()
            self._poll_thread_exc = None
            self._poll_thread = threading.Thread(target=self._poll)
            self._poll_thread.start()
            self._polling = True

    def stop_poll_thread(self) -> None:
        """Stop poll thread (if not already stopped)"""
        if self._poll_thread is not None and self._poll_thread.is_alive():
            self._stopevt.set()
            self._poll_thread.join()
            self.check_exception()
            self._polling = False

    def check_exception(self) -> None:
        """Raise any exception encountered in poller"""
        if self._poll_thread_exc is not None:
            # Reset sockets
            self._reset_sockets()
            # Then raise (in the current thread's context)
            raise self._poll_thread_exc

    def add_socket(self, uuid: UUID, address: str, port: int) -> None:
        """Add socket to pool"""
        with self._poller_lock:
            socket = self._context.socket(self._socket_type.value)
            socket.connect(f"tcp://{address}:{port}")
            self._poller.register(socket, zmq.POLLIN)
            self._sockets[uuid] = socket

    def remove_socket(self, uuid: UUID) -> None:
        """Remove socket from pool"""
        with self._poller_lock:
            try:
                socket = self._sockets.pop(uuid)
                self._poller.unregister(socket)
                socket.close()
            except KeyError:
                pass  # no socket for UUID registered

    def _reset_sockets(self) -> None:
        with self._poller_lock:
            for socket in self._sockets.values():
                self._poller.unregister(socket)
                socket.close()
            self._sockets.clear()

    def _poll(self) -> None:
        try:
            while not self._stopevt.is_set():
                with self._poller_lock:
                    sockets_ready = dict(self._poller.poll(timeout=50))
                for socket in sockets_ready.keys():
                    self._receive_cb(socket.recv_multipart())
                os.sched_yield()  # force-release lock
        except Exception as e:
            self._poll_thread_exc = e
            self._polling = False


class SubscriberPool(BasePool):
    """Subscription pool for ZeroMQ polling with PUB/SUB sockets"""

    def __init__(self, context: zmq.Context, receive_cb: Callable[[list[bytes]], None]):
        super().__init__(context, zmq.SocketType.SUB, receive_cb)

    def subscribe(self, topic: str, host: UUID | str | None = None) -> None:
        """Subscribe to a topic"""
        self._scribe(True, topic, host)

    def unsubscribe(self, topic: str, host: UUID | str | None = None) -> None:
        """Unsubscribe from a topic"""
        self._scribe(False, topic, host)

    def _scribe(self, subscribe: bool, topic: str, host: UUID | str | None = None) -> None:
        sockopt = zmq.SUBSCRIBE if subscribe else zmq.UNSUBSCRIBE
        if host is None:
            self._scribe_all(topic, sockopt)
        else:
            if isinstance(host, str):
                host = get_uuid(host)
            self._scribe_one(host, topic, sockopt)

    def _scribe_one(self, host: UUID, topic: str, sockopt: int):
        with self._poller_lock:
            if host in self._sockets:
                self._sockets[host].setsockopt_string(sockopt, topic)

    def _scribe_all(self, topic: str, sockopt: int):
        with self._poller_lock:
            for socket in self._sockets.values():
                socket.setsockopt_string(sockopt, topic)
