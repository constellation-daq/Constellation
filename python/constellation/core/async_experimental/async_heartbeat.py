import asyncio
import time
from collections.abc import Callable
from dataclasses import dataclass, field
from datetime import datetime
from uuid import UUID

import zmq.asyncio

from constellation.core.chp import CHPRole, chp_decode_message
from constellation.core.protocol.cscp1 import SatelliteState
from constellation.core.util import case_insensitive_dict

from .async_pools import AsyncSubscriberPool


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
    status: str = ""

    def refresh(self) -> None:
        self.last_refresh = time.monotonic()

    def seconds_since_refresh(self) -> float:
        return time.monotonic() - self.last_refresh


class AsyncHeartbeatReceiver:
    """Async heartbeat receiver backed by AsyncSubscriberPool.

    Socket management is delegated to an internal AsyncSubscriberPool.
    CHP has no topic filtering so the pool subscribes to "" for all sockets,
    applied automatically to every socket added via add_satellite().

    Call add_satellite() and remove_satellite() from the event loop.
    When calling from another thread use loop.call_soon_threadsafe().
    """

    INIT_LIVES = 3
    INIT_INTERVAL = 2000

    def __init__(
        self,
        ctx: zmq.asyncio.Context,
        on_state_change: Callable[[str, SatelliteState, SatelliteState], None] | None = None,
        on_satellite_dead: Callable[[str], None] | None = None,
    ) -> None:
        self._on_state_change = on_state_change
        self._on_satellite_dead = on_satellite_dead

        self._states: dict[UUID, HeartbeatState] = {}
        self._name_to_uuid: case_insensitive_dict[UUID] = case_insensitive_dict()

        self._pool = AsyncSubscriberPool(ctx, self._on_frames)
        self._pool.set_topics([""])

    def add_satellite(self, uuid: UUID, address: str, port: int, name: str) -> None:
        """Register a satellite for heartbeat tracking."""
        if uuid in self._states:
            return
        self._pool.add_socket(uuid, address, port)
        self._states[uuid] = HeartbeatState(
            host=uuid,
            name=name,
            lives=self.INIT_LIVES,
            interval_ms=self.INIT_INTERVAL,
        )
        self._name_to_uuid[name] = uuid

    def remove_satellite(self, uuid: UUID) -> None:
        """Deregister a satellite and close its socket."""
        self._pool.remove_socket(uuid)
        hb = self._states.pop(uuid, None)
        if hb is not None:
            self._name_to_uuid.pop(hb.name, None)

    @property
    def states(self) -> case_insensitive_dict[SatelliteState]:
        """Current states keyed by canonical name."""
        return case_insensitive_dict({hb.name: hb.state for hb in self._states.values()})

    @property
    def state_changes(self) -> case_insensitive_dict[datetime]:
        """Last state change timestamps keyed by canonical name."""
        return case_insensitive_dict({hb.name: hb.last_statechange for hb in self._states.values()})

    @property
    def statuses(self) -> case_insensitive_dict[str]:
        """Last status message keyed by canonical name."""
        return case_insensitive_dict({hb.name: hb.status for hb in self._states.values()})

    async def run(self, stop: asyncio.Event) -> None:
        """Run pool polling and stale check loop concurrently until stop is set."""
        await asyncio.gather(
            self._pool.run(stop),
            self._stale_check_loop(stop),
        )

    async def _stale_check_loop(self, stop: asyncio.Event) -> None:
        """Check for missed heartbeats every 300ms."""
        while not stop.is_set():
            await asyncio.sleep(0.3)
            self._check_stale_connections()

    def _on_frames(self, uuid: UUID, frames: list[bytes]) -> None:
        """Process a CHP frame delivered by the pool."""
        try:
            name, _timestamp, state_val, flags, interval, status = chp_decode_message(frames)
        except Exception:
            return

        hb = self._states.get(uuid)
        if hb is None:
            return

        if hb.name.casefold() != name.casefold():
            self._name_to_uuid.pop(hb.name, None)
            hb.name = name
            self._name_to_uuid[name] = uuid

        state = SatelliteState(state_val)

        if state != hb.state:
            old_state = hb.state
            hb.state = state
            hb.last_statechange = datetime.now()
            if self._on_state_change:
                self._on_state_change(name, old_state, state)

        hb.refresh()
        hb.interval_ms = interval
        hb.role = CHPRole.from_flags(flags)

        if status:
            hb.status = status

        if hb.lives != self.INIT_LIVES:
            hb.lives = self.INIT_LIVES

    def _check_stale_connections(self) -> None:
        """Decrement lives for satellites that have missed heartbeats."""
        for _uuid, hb in list(self._states.items()):
            if hb.lives <= 0:
                continue
            expected = (hb.interval_ms / 1000) * 1.5
            if hb.seconds_since_refresh() > expected:
                hb.lives -= 1
                if hb.lives == 0:
                    if self._on_satellite_dead:
                        self._on_satellite_dead(hb.name)
                    hb.state = SatelliteState.DEAD
                else:
                    hb.refresh()

    def close(self) -> None:
        """Close all sockets and clear state."""
        self._pool.close()
        self._states.clear()
        self._name_to_uuid.clear()
