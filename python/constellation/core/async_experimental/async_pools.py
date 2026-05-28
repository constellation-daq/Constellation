"""
SPDX-FileCopyrightText: 2026 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2
"""

import asyncio
from collections.abc import Callable
from uuid import UUID

import zmq
import zmq.asyncio


class AsyncSubscriberPool:
    """Async ZMQ SUB socket pool using zmq.asyncio.

    All methods must be called from the event loop. Use
    loop.call_soon_threadsafe() when calling from other threads.
    """

    def __init__(
        self,
        ctx: zmq.asyncio.Context,
        callback: Callable[[UUID, list[bytes]], None],
    ) -> None:
        self._ctx = ctx
        self._callback = callback
        self._sockets: dict[UUID, zmq.asyncio.Socket] = {}
        self._socket_to_uuid: dict[zmq.asyncio.Socket, UUID] = {}
        self._poller = zmq.asyncio.Poller()
        self._topics: list[str] = []

    def add_socket(self, uuid: UUID, address: str, port: int) -> None:
        """Add a subscriber socket and apply all current global topics."""
        if uuid in self._sockets:
            return
        sock = self._ctx.socket(zmq.SUB)
        sock.connect(f"tcp://{address}:{port}")
        sock.setsockopt(zmq.LINGER, 0)
        for topic in self._topics:
            sock.setsockopt_string(zmq.SUBSCRIBE, topic)
        self._poller.register(sock, zmq.POLLIN)
        self._sockets[uuid] = sock
        self._socket_to_uuid[sock] = uuid

    def remove_socket(self, uuid: UUID) -> None:
        """Remove and close the subscriber socket for a departing satellite."""
        sock = self._sockets.pop(uuid, None)
        if sock is not None:
            self._socket_to_uuid.pop(sock, None)
            self._poller.unregister(sock)
            sock.close()

    def subscribe(self, topic: str, uuid: UUID | None = None) -> None:
        """Subscribe to a topic on one or all sockets.

        When uuid is None, topic is added to _topics so future sockets added
        via add_socket() are automatically subscribed. When uuid is given,
        only that socket is subscribed and _topics is not modified.
        """
        if uuid is None:
            for sock in self._sockets.values():
                sock.setsockopt_string(zmq.SUBSCRIBE, topic)
            if topic not in self._topics:
                self._topics.append(topic)
        else:
            sock = self._sockets.get(uuid)
            if sock is not None:
                sock.setsockopt_string(zmq.SUBSCRIBE, topic)

    def unsubscribe(self, topic: str, uuid: UUID | None = None) -> None:
        """Unsubscribe from a topic on one or all sockets.

        When uuid is None, topic is removed from _topics. When uuid is given,
        only that socket is unsubscribed and _topics is not modified.
        """
        if uuid is None:
            for sock in self._sockets.values():
                sock.setsockopt_string(zmq.UNSUBSCRIBE, topic)
            if topic in self._topics:
                self._topics.remove(topic)
        else:
            sock = self._sockets.get(uuid)
            if sock is not None:
                sock.setsockopt_string(zmq.UNSUBSCRIBE, topic)

    def set_topics(self, new_topics: list[str]) -> None:
        """Replace all global subscriptions across all sockets."""
        old_set = set(self._topics)
        new_set = set(new_topics)
        for topic in old_set - new_set:
            for sock in self._sockets.values():
                sock.setsockopt_string(zmq.UNSUBSCRIBE, topic)
        for topic in new_set - old_set:
            for sock in self._sockets.values():
                sock.setsockopt_string(zmq.SUBSCRIBE, topic)
        self._topics = list(new_topics)

    async def run(self, stop: asyncio.Event) -> None:
        """Poll all sockets until stop is set."""
        while not stop.is_set():
            if not self._sockets:
                await asyncio.sleep(0.05)
                continue
            events = dict(await self._poller.poll(timeout=50))
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

    def close(self) -> None:
        """Close all sockets."""
        for sock in self._sockets.values():
            try:
                self._poller.unregister(sock)
            except Exception:
                pass
            sock.close()
        self._sockets.clear()
        self._socket_to_uuid.clear()
