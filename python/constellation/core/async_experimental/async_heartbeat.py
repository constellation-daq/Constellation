"""
SPDX-FileCopyrightText: 2026 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2

Async heartbeat receiver and checker.
"""

import asyncio
import time
from collections.abc import Callable
from dataclasses import dataclass, field
from datetime import datetime
from typing import Any
from uuid import UUID

import zmq.asyncio

from constellation.core.async_experimental.async_pools import AsyncSubscriberPool
from constellation.core.base import BaseSatelliteFrame
from constellation.core.chp import CHPMessageFlags, CHPRole, chp_decode_message
from constellation.core.protocol.cscp1 import SatelliteState
from constellation.core.util import case_insensitive_dict


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
    outdated: bool = False

    def refresh(self) -> None:
        self.last_refresh = time.monotonic()

    def seconds_since_refresh(self) -> float:
        return time.monotonic() - self.last_refresh


class AsyncHeartbeatReceiver:
    """Async heartbeat receiver backed by AsyncSubscriberPool.

    Manages per-satellite SUB sockets via an internal pool. CHP uses
    no topic filtering so each socket subscribes to all messages.
    """

    INIT_LIVES = 3
    INIT_INTERVAL = 2000

    def __init__(
        self,
        ctx: zmq.asyncio.Context,
        on_state_change: Callable[[str, SatelliteState, SatelliteState], None] | None = None,
        on_stale: Callable[[UUID], None] | None = None,
        on_mark_degraded: Callable[[str], None] | None = None,
        on_heartbeat_interrupt: Callable[[str], None] | None = None,
    ) -> None:
        self._on_state_change = on_state_change
        self._on_stale = on_stale
        self._on_mark_degraded = on_mark_degraded
        self._on_heartbeat_interrupt = on_heartbeat_interrupt

        self._states: dict[UUID, HeartbeatState] = {}
        self._name_to_uuid: case_insensitive_dict[UUID] = case_insensitive_dict()

        self._pool = AsyncSubscriberPool(ctx, self._on_frames)

    def add_satellite(self, uuid: UUID, address: str, port: int, name: str) -> None:
        """Register a satellite for heartbeat tracking."""
        if uuid in self._states:
            return
        self._pool.add_socket(uuid, address, port)
        self._pool.subscribe("", host=uuid)
        self._states[uuid] = HeartbeatState(
            host=uuid,
            name=name,
            lives=self.INIT_LIVES,
            interval_ms=self.INIT_INTERVAL,
        )
        self._name_to_uuid[name] = uuid

    def remove_satellite(self, satellite: UUID | str) -> None:
        """Deregister a satellite and close its socket."""
        if not isinstance(satellite, UUID):
            uuid = self._name_to_uuid.get(satellite)
            if not uuid:
                return
        else:
            uuid = satellite
        self._pool.remove_socket(uuid)
        hb = self._states.pop(uuid, None)
        if hb is not None:
            self._name_to_uuid.pop(hb.name, None)

    def remove_on_departure(self, uuid: UUID) -> None:
        """Deregister a satellite on explicit CHIRP departure.

        Checks MARK_DEGRADED and DENY_DEPARTURE flags before removal.
        """
        hb = self._states.get(uuid)
        if hb is not None:
            if hb.role.role_requires(CHPMessageFlags.MARK_DEGRADED) and self._on_mark_degraded:
                self._on_mark_degraded(f"{hb.name} departed illicitly")
            if hb.role.role_requires(CHPMessageFlags.DENY_DEPARTURE) and self._on_heartbeat_interrupt:
                self._on_heartbeat_interrupt(f"{hb.name} departed illicitly")
        self.remove_satellite(uuid)

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

        old_state = hb.state

        # Correct placeholder names with the canonical name from CHP payload
        renamed = hb.name.casefold() != name.casefold()
        if renamed:
            self._name_to_uuid.pop(hb.name, None)
            hb.name = name
            self._name_to_uuid[name] = uuid

        state = SatelliteState(state_val)
        hb.role = CHPRole.from_flags(flags)

        # Check for ERROR or SAFE state with TRIGGER_INTERRUPT flag
        call_interrupt = False
        if state in (SatelliteState.ERROR, SatelliteState.SAFE):
            call_interrupt = self._on_heartbeat_interrupt is not None and bool(flags & CHPMessageFlags.TRIGGER_INTERRUPT)

        state_changed = state != hb.state
        if state_changed:
            hb.state = state
            hb.last_statechange = datetime.now()
            hb.outdated = False

        if (state_changed or renamed) and self._on_state_change:
            self._on_state_change(name, old_state, hb.state)

        hb.refresh()
        hb.interval_ms = interval

        if status:
            hb.status = status

        # Replenish lives on heartbeat receipt regardless of state
        if hb.lives != self.INIT_LIVES:
            hb.lives = self.INIT_LIVES

        if call_interrupt:
            self._on_heartbeat_interrupt(f"{hb.name} reports state {state.name}")

    def _check_stale_connections(self) -> None:
        """Decrement lives for satellites that have missed heartbeats.

        When lives are exhausted the satellite is removed from tracking
        and on_stale fires so the controller can call forgetDiscoveredServices,
        matching Controller.cpp controller_loop.
        """
        newly_dead: list[UUID] = []
        for uuid, hb in self._states.items():
            if hb.lives <= 0:
                continue
            expected = (hb.interval_ms / 1000) * 1.5
            if hb.seconds_since_refresh() > expected:
                hb.lives -= 1
                if hb.lives == 0:
                    newly_dead.append(uuid)
                else:
                    hb.refresh()

        for uuid in newly_dead:
            hb = self._states.get(uuid)
            if hb is None:
                continue
            msg = f"No signs of life detected anymore from {hb.name}"
            if hb.role.role_requires(CHPMessageFlags.MARK_DEGRADED) and self._on_mark_degraded:
                self._on_mark_degraded(msg)
            if hb.role.role_requires(CHPMessageFlags.TRIGGER_INTERRUPT) and self._on_heartbeat_interrupt:
                self._on_heartbeat_interrupt(msg)
            self.remove_satellite(uuid)
            if self._on_stale:
                self._on_stale(uuid)

    def close(self) -> None:
        """Close all sockets and clear state."""
        self._pool.close()
        self._states.clear()
        self._name_to_uuid.clear()


class AsyncHeartbeatChecker(BaseSatelliteFrame):
    """Async equivalent of HeartbeatChecker.

    Owns an AsyncHeartbeatReceiver internally. Uses self._async_ctx from
    BaseSatelliteFrame. Callbacks fire on the event loop directly.
    """

    def __init__(self, **kwds: Any) -> None:
        super().__init__(**kwds)
        self._hb_receiver = AsyncHeartbeatReceiver(
            self._async_ctx,
            on_state_change=self._on_state_change,
            on_stale=self._on_heartbeat_stale,
            on_mark_degraded=self._mark_degraded,
            on_heartbeat_interrupt=self._heartbeat_interrupt,
        )

    def _add_com_task(self) -> None:
        """Register the async heartbeat receiver coroutine."""
        super()._add_com_task()
        self._com_task_factories.append(self._hb_receiver.run)

    def register_heartbeat_host(
        self,
        uuid: UUID,
        address: str,
        port: int,
        name: str,
    ) -> None:
        """Register a satellite for heartbeat tracking."""
        self._hb_receiver.add_satellite(uuid, address, port, name)

    def unregister_heartbeat_host(self, uuid: UUID) -> None:
        """Deregister a satellite from heartbeat tracking on explicit departure."""
        self._hb_receiver.remove_on_departure(uuid)

    def forget_heartbeat_host(self, satellite: str) -> None:
        """Remove a satellite from heartbeat tracking by name immediately."""
        self._hb_receiver.remove_satellite(satellite)

    @property
    def heartbeat_states(self) -> case_insensitive_dict[SatelliteState]:
        """Current states keyed by canonical name."""
        return self._hb_receiver.states

    @property
    def heartbeat_state_changes(self) -> case_insensitive_dict[datetime]:
        """Last state change timestamps keyed by canonical name."""
        return self._hb_receiver.state_changes

    @property
    def heartbeat_statuses(self) -> case_insensitive_dict[str]:
        """Last status messages keyed by canonical name."""
        return self._hb_receiver.statuses

    def _on_state_change(
        self,
        name: str,
        old_state: SatelliteState,
        new_state: SatelliteState,
    ) -> None:
        """Called when a satellite changes state. Override in subclass."""

    def _on_heartbeat_stale(self, uuid: UUID) -> None:
        """Called when a satellite's heartbeat lives are exhausted.

        Override in subclass to propagate removal to CHIRP.
        """

    def _mark_degraded(self, reason: str) -> None:
        """Called when marking the run as degraded. Override in subclass."""

    def _heartbeat_interrupt(self, reason: str) -> None:
        """Called when triggering an interrupt. Override in subclass."""

    def close(self) -> None:
        """Close all heartbeat sockets."""
        self._hb_receiver.close()
