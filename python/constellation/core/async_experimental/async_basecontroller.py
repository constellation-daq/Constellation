"""
SPDX-FileCopyrightText: 2026 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2

Async base and scriptable controller framework classes.
"""

import asyncio
import threading
from concurrent.futures import ThreadPoolExecutor
from typing import Any
from uuid import UUID

import zmq

from ..controller import SatelliteUpdate
from ..cscp import CommandTransmitter
from ..logging import setup_cli_logging
from .async_chirp import CHIRPEvent, DiscoveredService
from .async_heartbeatchecker import AsyncHeartbeatChecker
from .async_monitoringlistener import AsyncMonitoringListener


class AsyncBaseController(AsyncMonitoringListener, AsyncHeartbeatChecker):
    """Async equivalent of BaseController.

    CSCP commands run in ThreadPoolExecutor(max_workers=1) to keep
    blocking REQ socket operations off the event loop.
    Discovery and heartbeat run as coroutines.
    """

    def __init__(self, loop: asyncio.AbstractEventLoop, **kwds: Any) -> None:
        super().__init__(**kwds)
        self._loop = loop
        self._transmitters: dict[str, CommandTransmitter] = {}
        self._transmitter_uuids: dict[UUID, str] = {}
        self._transmitter_lock = threading.Lock()
        self._cscp_executor = ThreadPoolExecutor(max_workers=1)
        self._sync_ctx = zmq.Context()

        self.register_chirp_callback("basecontroller_control", self._on_control_service)
        self.register_chirp_callback("basecontroller_heartbeat", self._on_heartbeat_service)

    def _on_control_service(self, event: CHIRPEvent, service: DiscoveredService) -> None:
        """Handle CONTROL service connect/disconnect."""
        if event == CHIRPEvent.SERVICE_CONNECTED:
            asyncio.ensure_future(self._setup_transmitter(service.host_id, service.addresses[0], service.port))
        elif event == CHIRPEvent.SERVICE_DISCONNECTED:
            self._cleanup_transmitter(service.host_id)

    def _on_heartbeat_service(self, event: CHIRPEvent, service: DiscoveredService) -> None:
        """Handle HEARTBEAT service connect/disconnect."""
        if event == CHIRPEvent.SERVICE_CONNECTED:
            name = self._transmitter_uuids.get(service.host_id, f"Unknown-{str(service.host_id)[:8]}")
            self.register_heartbeat_host(service.host_id, service.addresses[0], service.port, name)
        elif event == CHIRPEvent.SERVICE_DISCONNECTED:
            self.unregister_heartbeat_host(service.host_id)

    async def command(
        self,
        cmd: str,
        sat: str = "",
        satcls: str = "",
        payload: Any = None,
    ) -> str:
        """Send a CSCP command via the dedicated executor."""

        def _send() -> str:
            key = f"{satcls}.{sat}".strip(".")
            with self._transmitter_lock:
                ct = self._transmitters.get(key)
            if ct is None:
                return f"ERROR: No transmitter for {key}"
            try:
                msg = ct.request_get_response(command=cmd, payload=payload)
                return f"{msg.verb_msg}"
            except Exception as e:
                return f"ERROR: {e}"

        return await self._loop.run_in_executor(self._cscp_executor, _send)

    def _on_satellite_update(self, name: str, update_type: SatelliteUpdate) -> None:
        """Called on satellite connect/disconnect. Override in subclass."""

    async def _setup_transmitter(self, uuid: UUID, address: str, port: int) -> None:
        """Connect to a satellite CSCP port in the executor."""

        def _connect():
            sock = self._sync_ctx.socket(zmq.REQ)
            sock.connect(f"tcp://{address}:{port}")
            sock.setsockopt(zmq.LINGER, 2000)
            sock.setsockopt(zmq.RCVTIMEO, 5000)
            try:
                ct = CommandTransmitter(self.name, sock)
                msg = ct.request_get_response("get_commands")
                return msg.sender, ct
            except Exception:
                sock.close()
                return None

        result = await self._loop.run_in_executor(self._cscp_executor, _connect)
        if result is None:
            return
        canonical_name, ct = result
        with self._transmitter_lock:
            self._transmitters[canonical_name] = ct
            self._transmitter_uuids[uuid] = canonical_name
        self._on_satellite_update(canonical_name, SatelliteUpdate.ADDED)

    def _cleanup_transmitter(self, uuid: UUID) -> None:
        """Remove and close a CSCP transmitter."""
        with self._transmitter_lock:
            canonical_name = self._transmitter_uuids.pop(uuid, None)
            if canonical_name is None:
                return
            ct = self._transmitters.pop(canonical_name, None)
        if ct is not None:
            ct.close()
            self._on_satellite_update(canonical_name, SatelliteUpdate.REMOVED)

    def shutdown(self) -> None:
        """Shut down all components."""
        self._cscp_executor.shutdown(wait=True)
        self.close()
        with self._transmitter_lock:
            for ct in self._transmitters.values():
                ct.close()
        self._sync_ctx.term()


class AsyncScriptableController(AsyncBaseController):
    """Async equivalent of ScriptableController."""

    def __init__(self, log_level: str = "INFO", **kwds: Any) -> None:
        setup_cli_logging(log_level)
        super().__init__(**kwds)
