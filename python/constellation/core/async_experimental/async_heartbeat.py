import asyncio
import time
from dataclasses import dataclass, field
from datetime import datetime
from typing import Callable
from uuid import UUID

import zmq
import zmq.asyncio

from constellation.core.chp import CHPRole, chp_decode_message
from constellation.core.protocol.cscp1 import SatelliteState


@dataclass
class HeartbeatState:
    """Tracked state for a single satellite."""
    host: UUID
    name: str
    state: SatelliteState = SatelliteState.DEAD
    last_refresh: float = field(default_factory=time.monotonic)
    last_statechange: datetime = field(default_factory=datetime.now)
    interval_ms: int = 2000
    lives: int = 3
    role: CHPRole = CHPRole.DYNAMIC

    def refresh(self) -> None:
        self.last_refresh = time.monotonic()

    def seconds_since_refresh(self) -> float:
        return time.monotonic() - self.last_refresh


class AsyncHeartbeatReceiver:
    """Async heartbeat receiver with lives/stale connection tracking. Call add_satellite() via call_soon_threadsafe() from threads."""

    INIT_LIVES = 3
    INIT_INTERVAL = 2000

    def __init__(
        self,
        ctx: zmq.asyncio.Context,
        on_state_change: Callable[[str, SatelliteState, SatelliteState], None] | None = None,
        on_satellite_dead: Callable[[str], None] | None = None,
    ):
        self._ctx = ctx
        self._on_state_change = on_state_change
        self._on_satellite_dead = on_satellite_dead

        self._sockets: dict[UUID, zmq.asyncio.Socket] = {}
        self._states: dict[UUID, HeartbeatState] = {}
        self._poller = zmq.asyncio.Poller()

    def add_satellite(self, uuid: UUID, address: str, port: int, name: str) -> None:
        """Add a satellite to track. Called from CHIRP discovery."""
        if uuid in self._sockets:
            return

        sock = self._ctx.socket(zmq.SUB)
        sock.connect(f"tcp://{address}:{port}")
        sock.setsockopt_string(zmq.SUBSCRIBE, "")
        sock.setsockopt(zmq.LINGER, 0)

        self._poller.register(sock, zmq.POLLIN)
        self._sockets[uuid] = sock
        self._states[uuid] = HeartbeatState(
            host=uuid,
            name=name,
            lives=self.INIT_LIVES,
            interval_ms=self.INIT_INTERVAL,
        )

    def remove_satellite(self, uuid: UUID) -> None:
        """Remove a satellite from tracking."""
        sock = self._sockets.pop(uuid, None)
        if sock is not None:
            self._poller.unregister(sock)
            sock.close()
        self._states.pop(uuid, None)

    @property
    def states(self) -> dict[str, SatelliteState]:
        """Get current states keyed by lowercase name."""
        return {hb.name.lower(): hb.state for hb in self._states.values()}

    @property
    def state_changes(self) -> dict[str, datetime]:
        """Get last state change times keyed by name."""
        return {hb.name: hb.last_statechange for hb in self._states.values()}

    async def run(self, stop: asyncio.Event) -> None:
        """Main polling loop. Runs until stop is set."""
        last_check = time.monotonic()

        while not stop.is_set():
            if self._sockets:
                events = dict(await self._poller.poll(timeout=50))
                for uuid, sock in list(self._sockets.items()):
                    if sock in events:
                        try:
                            msg = await sock.recv_multipart()
                            self._process_heartbeat(uuid, msg)
                        except zmq.Again:
                            pass

                now = time.monotonic()
                if (now - last_check) > 0.3:
                    self._check_stale_connections()
                    last_check = now
            else:
                await asyncio.sleep(0.05)

    def _process_heartbeat(self, uuid: UUID, msg: list[bytes]) -> None:
        """Process a received heartbeat message."""
        try:
            name, timestamp, state_val, flags, interval, status = chp_decode_message(msg)
            state = SatelliteState(state_val)

            hb = self._states.get(uuid)
            if hb is None:
                return

            if hb.name != name:
                hb.name = name

            if state != hb.state:
                old_state = hb.state
                hb.state = state
                hb.last_statechange = datetime.now()
                if self._on_state_change:
                    self._on_state_change(name, old_state, state)

            hb.refresh()
            hb.interval_ms = interval
            hb.role = CHPRole.from_flags(flags)

            if hb.lives != self.INIT_LIVES:
                hb.lives = self.INIT_LIVES

        except Exception:
            pass

    def _check_stale_connections(self) -> None:
        """Check for satellites that have missed heartbeats."""
        for uuid, hb in list(self._states.items()):
            if hb.lives <= 0:
                continue

            expected_interval = (hb.interval_ms / 1000) * 1.5
            if hb.seconds_since_refresh() > expected_interval:
                hb.lives -= 1

                if hb.lives == 0:
                    if self._on_satellite_dead:
                        self._on_satellite_dead(hb.name)
                    hb.state = SatelliteState.DEAD
                else:
                    hb.refresh()

    def close(self) -> None:
        """Close all sockets."""
        for sock in self._sockets.values():
            try:
                self._poller.unregister(sock)
            except Exception:
                pass
            sock.close()
        self._sockets.clear()
        self._states.clear()
