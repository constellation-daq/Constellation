"""
SPDX-FileCopyrightText: 2026 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2

Async ZMQ socket pools following the C++ BasePool/SubscriberPool hierarchy.

AsyncBasePool handles socket lifecycle and async polling for any ZMQ socket
type. AsyncSubscriberPool adds SUB-specific subscribe and unsubscribe
operations. Neither class tracks subscription state; that responsibility
belongs to higher layers such as AsyncCMDPListener.
"""

import asyncio
import logging
from collections.abc import Callable
from uuid import UUID

import zmq
import zmq.asyncio

_log = logging.getLogger(__name__)


class AsyncBasePool:
    """Async ZMQ socket pool with polling.

    Manages a set of sockets keyed by host UUID, polls them for incoming
    multipart messages, and dispatches to a callback. The socket type is
    determined at construction, allowing reuse for SUB, PULL, or other
    patterns.

    All methods must be called from the event loop thread.
    """

    def __init__(
        self,
        ctx: zmq.asyncio.Context,
        socket_type: int,
        callback: Callable[[UUID, list[bytes]], None],
    ) -> None:
        self._ctx = ctx
        self._socket_type = socket_type
        self._callback = callback
        self._sockets: dict[UUID, zmq.asyncio.Socket] = {}
        self._socket_to_uuid: dict[zmq.asyncio.Socket, UUID] = {}
        self._poller = zmq.asyncio.Poller()

    @property
    def sockets(self) -> dict[UUID, zmq.asyncio.Socket]:
        """Direct access for subclasses that need to manipulate sockets."""
        return self._sockets

    def add_socket(self, uuid: UUID, address: str, port: int) -> None:
        """Connect a new socket and register it with the poller.

        Calls host_connected after registration so subclasses can apply
        initial subscriptions or other socket options to the new socket.
        """
        if uuid in self._sockets:
            return
        sock = self._ctx.socket(self._socket_type)
        sock.connect(f"tcp://{address}:{port}")
        sock.setsockopt(zmq.LINGER, 0)
        self._poller.register(sock, zmq.POLLIN)
        self._sockets[uuid] = sock
        self._socket_to_uuid[sock] = uuid
        self.host_connected(uuid)

    def remove_socket(self, uuid: UUID) -> None:
        """Disconnect and close a socket for a departing host.

        Calls host_disconnected before closing so subclasses can clean
        up per-host state while the socket reference is still valid.
        """
        sock = self._sockets.pop(uuid, None)
        if sock is None:
            return
        self.host_disconnected(uuid)
        self._socket_to_uuid.pop(sock, None)
        try:
            self._poller.unregister(sock)
        except Exception:
            pass
        sock.close()

    def host_connected(self, uuid: UUID) -> None:
        """Called after a new socket is registered. Override in subclass."""

    def host_disconnected(self, uuid: UUID) -> None:
        """Called before a socket is closed on departure. Override in subclass."""

    async def run(self, stop: asyncio.Event) -> None:
        """Poll all registered sockets until stop is set."""
        while not stop.is_set():
            if not self._sockets:
                await asyncio.sleep(0.05)
                continue
            try:
                events = dict(await self._poller.poll(timeout=50))
            except asyncio.CancelledError:
                if stop.is_set():
                    break
                continue
            for sock in events:
                uuid = self._socket_to_uuid.get(sock)
                if uuid is None:
                    continue
                try:
                    msg = await sock.recv_multipart()
                    self._callback(uuid, msg)
                    while sock.getsockopt(zmq.EVENTS) & zmq.POLLIN:
                        msg = await sock.recv_multipart()
                        self._callback(uuid, msg)
                except zmq.ZMQError:
                    pass
                except Exception:
                    _log.exception("Unhandled error dispatching message from %s", uuid)

    def close(self) -> None:
        """Close all sockets and clear internal state."""
        for sock in self._sockets.values():
            try:
                self._poller.unregister(sock)
            except Exception:
                pass
            sock.close()
        self._sockets.clear()
        self._socket_to_uuid.clear()


class AsyncSubscriberPool(AsyncBasePool):
    """Async ZMQ SUB socket pool.

    Extends AsyncBasePool with subscribe and unsubscribe operations on
    individual or all sockets. Does not track subscription state; callers
    are responsible for ensuring balanced subscribe/unsubscribe pairs.
    """

    def __init__(
        self,
        ctx: zmq.asyncio.Context,
        callback: Callable[[UUID, list[bytes]], None],
    ) -> None:
        super().__init__(ctx, zmq.SUB, callback)

    def subscribe(self, topic: str, host: UUID | str | None = None) -> None:
        """Subscribe to a topic on one or all sockets.

        When host is None the topic is applied to every connected socket.
        When host is a UUID, only that socket is subscribed. When host is
        a string (canonical name), it is resolved to a UUID via MD5 hash.
        """
        if host is None:
            self._scribe_all(topic, zmq.SUBSCRIBE)
        else:
            if isinstance(host, str):
                from constellation.core.chirp import get_uuid

                host = get_uuid(host)
            self._scribe_one(host, topic, zmq.SUBSCRIBE)

    def unsubscribe(self, topic: str, host: UUID | str | None = None) -> None:
        """Unsubscribe from a topic on one or all sockets."""
        if host is None:
            self._scribe_all(topic, zmq.UNSUBSCRIBE)
        else:
            if isinstance(host, str):
                from constellation.core.chirp import get_uuid

                host = get_uuid(host)
            self._scribe_one(host, topic, zmq.UNSUBSCRIBE)

    def _scribe_one(self, host_id: UUID, topic: str, sockopt: int) -> None:
        sock = self._sockets.get(host_id)
        if sock is not None:
            sock.setsockopt_string(sockopt, topic)

    def _scribe_all(self, topic: str, sockopt: int) -> None:
        for sock in self._sockets.values():
            sock.setsockopt_string(sockopt, topic)
