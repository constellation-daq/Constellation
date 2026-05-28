"""
SPDX-FileCopyrightText: 2026 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2

Async CHIRP manager framework class.
"""

import asyncio
from typing import Any

from ..base import BaseSatelliteFrame
from ..chirp import CHIRPServiceIdentifier
from ..network import get_interface_addresses
from .async_chirp import AsyncCHIRPManager as _AsyncCHIRPManagerImpl
from .async_chirp import DiscoveredService


class AsyncCHIRPManager(BaseSatelliteFrame):
    """Async equivalent of CHIRPManager.

    Uses AsyncCHIRPManager from async_chirp internally.
    Registers CHIRP discovery as a coroutine via _add_com_task.
    """

    def __init__(
        self,
        name: str,
        group: str,
        interface: list[str] | None,
        **kwds: Any,
    ) -> None:
        super().__init__(name=name, **kwds)
        self.group = group
        interface_addresses = get_interface_addresses(interface)
        self._chirp = _AsyncCHIRPManagerImpl(
            name=self.name,
            group=group,
            interface_addresses=interface_addresses,
        )

    def _add_com_task(self) -> None:
        """Register the async CHIRP discovery coroutine."""
        super()._add_com_task()
        self._com_task_factories.append(self._run_chirp)

    async def _run_chirp(self, stop: asyncio.Event) -> None:
        """Start CHIRP discovery and run until stop is set."""
        loop = asyncio.get_running_loop()
        await self._chirp.start(loop)
        await stop.wait()
        self._chirp.close()

    def register_chirp_callback(
        self,
        callback_id: str,
        callback_func,
    ) -> None:
        """Register a callback for CHIRP service events."""
        self._chirp.register_callback(callback_id, callback_func)

    def unregister_chirp_callback(self, callback_id: str) -> None:
        """Remove a previously registered CHIRP callback."""
        self._chirp.unregister_callback(callback_id)

    def request(self, service_id: CHIRPServiceIdentifier) -> None:
        """Send a CHIRP REQUEST for a specific service type."""
        self._chirp.request(service_id)

    def get_discovered(self, service_id: CHIRPServiceIdentifier) -> list[DiscoveredService]:
        """Return discovered services matching service_id."""
        return self._chirp.get_discovered(service_id)
