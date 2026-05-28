"""
SPDX-FileCopyrightText: 2026 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2

Async heartbeat checker framework class.
"""

from typing import Any

import zmq.asyncio

from ..base import BaseSatelliteFrame
from ..protocol.cscp1 import SatelliteState
from .async_heartbeat import AsyncHeartbeatReceiver


class AsyncHeartbeatChecker(BaseSatelliteFrame):
    """Async equivalent of HeartbeatChecker.

    Uses AsyncHeartbeatReceiver internally.
    Callbacks fire on the event loop — no call_soon_threadsafe needed.
    """

    def __init__(self, **kwds: Any) -> None:
        super().__init__(**kwds)
        self._async_ctx = zmq.asyncio.Context()
        self._hb_receiver = AsyncHeartbeatReceiver(
            self._async_ctx,
            on_state_change=self._on_state_change,
            on_satellite_dead=self._on_satellite_dead,
        )

    def _add_com_task(self) -> None:
        """Register the async heartbeat receiver coroutine."""
        super()._add_com_task()
        self._com_task_factories.append(self._hb_receiver.run)

    def register_heartbeat_host(
        self,
        uuid,
        address: str,
        port: int,
        name: str,
    ) -> None:
        """Register a satellite for heartbeat tracking."""
        self._hb_receiver.add_satellite(uuid, address, port, name)

    def unregister_heartbeat_host(self, uuid) -> None:
        """Deregister a satellite from heartbeat tracking."""
        self._hb_receiver.remove_satellite(uuid)

    @property
    def heartbeat_states(self):
        """Current states keyed by canonical name."""
        return self._hb_receiver.states

    def _on_state_change(
        self,
        name: str,
        old_state: SatelliteState,
        new_state: SatelliteState,
    ) -> None:
        """Called when a satellite changes state. Override in subclass."""

    def _on_satellite_dead(self, name: str) -> None:
        """Called when a satellite stops sending heartbeats. Override in subclass."""

    def close(self) -> None:
        """Close all heartbeat sockets."""
        self._hb_receiver.close()
        self._async_ctx.term()
