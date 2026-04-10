import asyncio
from collections.abc import Callable
from uuid import UUID

import zmq
import zmq.asyncio


class AsyncSubscriberPool:
    """Async ZMQ subscriber pool. Call all methods from the event loop; use call_soon_threadsafe() from threads."""

    def __init__(
        self,
        ctx: zmq.asyncio.Context,
        callback: Callable[[list[bytes]], None],
    ) -> None:
        self._ctx = ctx
        self._callback = callback
        self._sockets: dict[UUID, zmq.asyncio.Socket] = {}
        self._poller = zmq.asyncio.Poller()
        self._topics: list[str] = []

    def add_socket(self, uuid: UUID, address: str, port: int) -> None:
        """Add a subscriber socket for a discovered satellite."""
        if uuid in self._sockets:
            return
        sock = self._ctx.socket(zmq.SUB)
        sock.connect(f"tcp://{address}:{port}")
        for topic in self._topics:
            sock.setsockopt_string(zmq.SUBSCRIBE, topic)
        self._poller.register(sock, zmq.POLLIN)
        self._sockets[uuid] = sock

    def remove_socket(self, uuid: UUID) -> None:
        """Remove a subscriber socket for a departing satellite."""
        sock = self._sockets.pop(uuid, None)
        if sock is not None:
            self._poller.unregister(sock)
            sock.close()

    def set_topics(self, new_topics: list[str]) -> None:
        """Replace all topic subscriptions across all sockets."""
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
        """Poll loop. Runs until stop is set."""
        while not stop.is_set():
            if not self._sockets:
                try:
                    await asyncio.wait_for(stop.wait(), timeout=0.1)
                    break
                except asyncio.TimeoutError:
                    continue
            events = dict(await self._poller.poll(timeout=200))
            for sock in events:
                msg = await sock.recv_multipart()
                self._callback(msg)

    def close(self) -> None:
        """Close all sockets."""
        for sock in self._sockets.values():
            try:
                self._poller.unregister(sock)
            except Exception:
                pass
            sock.close()
        self._sockets.clear()
