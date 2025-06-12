"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2
"""

from __future__ import annotations

import threading
import time
from datetime import datetime, timezone
from enum import Enum, auto
from typing import Any, Callable, Optional
from uuid import UUID

import zmq

from .base import BaseSatelliteFrame
from .chp import CHPDecodeMessage, CHPMessageFlags
from .fsm import SatelliteState


class HeartbeatRole(Enum):
    """Defines the role of the satellite."""

    NONE = auto()
    TRANSIENT = auto()
    DYNAMIC = auto()
    ESSENTIAL = auto()

    @classmethod
    def from_flags(cls, flags: CHPMessageFlags) -> HeartbeatRole:
        if flags & CHPMessageFlags.MARK_DEGRADED:
            if flags & CHPMessageFlags.TRIGGER_INTERRUPT:
                if flags & CHPMessageFlags.DENY_DEPARTURE:
                    return HeartbeatRole.ESSENTIAL
                return HeartbeatRole.DYNAMIC
            return HeartbeatRole.TRANSIENT
        return HeartbeatRole.NONE


def role_requires(role: HeartbeatRole, flags: CHPMessageFlags) -> bool:
    if role == HeartbeatRole.TRANSIENT:
        return bool(flags & (CHPMessageFlags.MARK_DEGRADED))
    if role == HeartbeatRole.DYNAMIC:
        return bool(flags & (CHPMessageFlags.MARK_DEGRADED | CHPMessageFlags.TRIGGER_INTERRUPT))
    if role == HeartbeatRole.ESSENTIAL:
        return bool(
            flags & (CHPMessageFlags.MARK_DEGRADED | CHPMessageFlags.TRIGGER_INTERRUPT | CHPMessageFlags.DENY_DEPARTURE)
        )
    return False


class HeartbeatState:
    def __init__(
        self,
        host: UUID,
        name: str,
        evt: threading.Event,
        lives: int,
        interval: int,
        role: HeartbeatRole = HeartbeatRole.DYNAMIC,
    ):
        self.host = host
        self.name = name
        self.role = role
        self.lives = lives
        self.interval = interval
        self.last_refresh = datetime.now(timezone.utc)
        self.state = SatelliteState.NEW
        self.failed: threading.Event = evt

    def refresh(self, ts: datetime | None = None) -> None:
        if not ts:
            self.last_refresh = datetime.now(timezone.utc)
        else:
            self.last_refresh = ts

    @property
    def seconds_since_refresh(self) -> float:
        return (datetime.now(timezone.utc) - self.last_refresh).total_seconds()


class HeartbeatChecker(BaseSatelliteFrame):
    """Checks periodically Satellites' state via subscription to its Heartbeat.

    Individual heartbeat checks run in separate threads. In case of a failure
    (either the Satellite is in ERROR/SAFE state or has missed several
    heartbeats) the corresponding thread will set a `failed` event.
    Alternatively, an action can be triggered via a callback.

    """

    # initial values for period and lives
    HB_INIT_LIVES = 3
    HB_INIT_PERIOD = 2000

    def __init__(self, *args: Any, **kwargs: Any):
        super().__init__(*args, **kwargs)

        self.log_chp = self.get_logger("CHP")

        self._callback_lock = threading.Lock()
        self._poller = zmq.Poller()
        # dict to keep states mapped to socket
        self._states = dict[zmq.Socket, HeartbeatState]()  # type: ignore[type-arg]
        self._socket_lock = threading.Lock()
        self.auto_recover = False  # clear fail Event if Satellite reappears?

    def register_heartbeat_callback(self, callback: Optional[Callable[[str, SatelliteState], None]] = None) -> None:
        self._callback = callback

    def _add_com_thread(self) -> None:
        """Add the heartbeat listener thread to the communication thread pool."""
        super()._add_com_thread()
        self._com_thread_pool["heartbeatrecv"] = threading.Thread(target=self._run_thread, daemon=True)
        self.log_chp.debug("Heartbeat receiver thread prepared and added to the pool.")

    def register_heartbeat_host(
        self, host: UUID, address: str, name: str = "", context: Optional[zmq.Context] = None  # type: ignore[type-arg]
    ) -> threading.Event:
        """Register a heartbeat check for a specific Satellite.

        Returns threading.Event that will be set when a failure occurs.

        """
        for hb in self._states.values():
            if host == hb.host:
                self.log_chp.warning(f"Heartbeating for {host} already registered!")
                return hb.failed

        ctx = context or zmq.Context()
        try:
            socket = ctx.socket(zmq.SUB)
        except zmq.ZMQError as e:
            if "Too many open files" in e.strerror:
                self.log_chp.error(
                    "System reports too many open files: cannot open further connections.\n"
                    "Please consider increasing the limit of your OS."
                    "On Linux systems, use 'ulimit' to set a higher value."
                )
            raise e
        socket.connect(address)
        socket.setsockopt_string(zmq.SUBSCRIBE, "")
        evt = threading.Event()
        self._states[socket] = HeartbeatState(host, name, evt, self.HB_INIT_LIVES, self.HB_INIT_PERIOD)

        with self._socket_lock:
            self._poller.register(socket, zmq.POLLIN)

        self.log_chp.info(f"Registered heartbeating check for {address}")
        return evt

    def unregister_heartbeat_host(self, host: UUID) -> None:
        """Unregister a heartbeat check for a specific Satellite."""
        s: zmq.Socket | None = None  # type: ignore[type-arg]
        name: str | None = None
        for socket, hb in self._states.items():
            if hb.host == host:
                # TODO(simonspa) Add DENY_DEPARTURE check here and call _interrupting
                s = socket
                name = hb.name
                break
        if not s:
            return
        with self._socket_lock:
            self._poller.unregister(s)
            self._states.pop(s)
            s.close()
        self.log_chp.info("Removed heartbeat check for %s", name)

    def heartbeat_host_is_registered(self, host: UUID) -> bool:
        """Check whether a given Satellite is already registered."""
        registered = False
        for hb in self._states.values():
            if hb.host == host:
                registered = True
                break
        return registered

    @property
    def heartbeat_states(self) -> dict[str, SatelliteState]:
        """Return a dictionary of the monitored Satellites' state."""
        res = {}
        for hb in self._states.values():
            res[hb.name] = hb.state
        return res

    @property
    def fail_events(self) -> dict[str, threading.Event]:
        """Return a dictionary of Events triggered for failed Satellites."""
        res = {}
        for hb in self._states.values():
            if hb.failed.is_set():
                res[hb.name] = hb.failed
        return res

    def _run_thread(self) -> None:
        self.log_chp.info("Starting heartbeat check thread")
        last_check = datetime.now(timezone.utc)

        # refresh all tokens
        for hb in self._states.values():
            hb.refresh()

        # assert for mypy static type analysis
        assert isinstance(self._com_thread_evt, threading.Event), "Thread Event not set up correctly"

        while not self._com_thread_evt.is_set():
            # check for heartbeats ready to be received
            with self._socket_lock:
                sockets_ready = dict(self._poller.poll(timeout=50))
                for socket in sockets_ready.keys():
                    binmsg = socket.recv_multipart()
                    name, timestamp, state, flags, interval, status = CHPDecodeMessage(binmsg)
                    self.log_chp.debug(f"Received heartbeat from {name}, state {state}, next in {interval}")
                    hb = self._states[socket]
                    # update values
                    hb.name = name
                    hb.refresh(timestamp.to_datetime())
                    hb.state = SatelliteState(state)
                    hb.interval = interval
                    hb.role = HeartbeatRole.from_flags(flags)
                    # refresh lives
                    if hb.lives != self.HB_INIT_LIVES:
                        self.log_chp.log(
                            5,
                            "%s had %d lives left (interval %d), refreshing",
                            hb.name,
                            hb.lives,
                            hb.interval,
                        )
                    hb.lives = self.HB_INIT_LIVES
                    if (
                        hb.state
                        in [
                            SatelliteState.ERROR,
                            SatelliteState.SAFE,
                            SatelliteState.DEAD,
                        ]
                        and flags & CHPMessageFlags.TRIGGER_INTERRUPT
                    ):
                        # satellite in error state, interrupt
                        if not hb.failed.is_set():
                            self.log_chp.info(f"{hb.name} state causing interrupt callback to be called")
                            hb.failed.set()
                            self._interrupting(hb.name, hb.state)
                    else:
                        # recover?
                        if self.auto_recover and hb.failed.is_set():
                            hb.failed.clear()

            # regularly check for stale connections and missed heartbeats
            if (datetime.now(timezone.utc) - last_check).total_seconds() > 0.3:
                for hb in self._states.values():
                    if hb.seconds_since_refresh > (hb.interval / 1000) * 1.5 and not hb.failed.is_set():
                        # no message after 150% of the interval, subtract life
                        hb.lives -= 1
                        self.log_chp.log(
                            5,
                            "%s unresponsive, removed life, now %d",
                            hb.name,
                            hb.lives,
                        )
                        if hb.lives <= 0 and role_requires(hb.role, CHPMessageFlags.TRIGGER_INTERRUPT):
                            # no lives left, interrupt
                            if not hb.failed.is_set():
                                self.log_chp.info(f"{hb.name} unresponsive causing interrupt callback to be called")
                                hb.failed.set()
                                self._interrupting(hb.name, SatelliteState.DEAD)
                                # update state
                                hb.state = SatelliteState.DEAD
                            # try again later
                            hb.refresh()
                        else:
                            # refresh, try again later
                            hb.refresh()
                # update timestamp for this round
                last_check = datetime.now(timezone.utc)
            # finally, wait a moment
            time.sleep(0.05)

    def _interrupting(self, name: str, state: SatelliteState) -> None:
        with self._callback_lock:
            if hasattr(self, "_callback") and self._callback:
                try:
                    self._callback(name, state)
                except Exception:
                    pass

    def get_failed(self) -> list[str]:
        """Get a list of the names of all failed Satellites."""
        res = list[str]()
        for hb in self._states.values():
            if hb.failed.is_set():
                res.append(hb.name)
        return res

    def close(self) -> None:
        for socket in self._states.keys():
            socket.close()
        self._states = dict[zmq.Socket, HeartbeatState]()  # type: ignore[type-arg]
