"""
SPDX-FileCopyrightText: 2026 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2

Async base and scriptable controller framework classes.
"""

import asyncio
from typing import Any
from uuid import UUID

import zmq
import zmq.asyncio

from constellation.core.async_experimental.async_chirp import CHIRPEvent, DiscoveredService
from constellation.core.async_experimental.async_heartbeat import AsyncHeartbeatChecker
from constellation.core.async_experimental.async_monitoringlistener import AsyncMonitoringListener
from constellation.core.chirp import CHIRPServiceIdentifier
from constellation.core.controller import SatelliteUpdate
from constellation.core.logging import setup_cli_logging
from constellation.core.message.cscp1 import CSCP1Message


class AsyncBaseController(AsyncMonitoringListener, AsyncHeartbeatChecker):
    """Async equivalent of BaseController.

    CSCP commands use zmq.asyncio with await send/recv directly, eliminating
    the dedicated executor and synchronous context. _async_ctx from
    BaseSatelliteFrame is used for all ZMQ sockets including CSCP REQ sockets.
    Discovery and heartbeat run as coroutines.
    """

    def __init__(
        self,
        loop: asyncio.AbstractEventLoop | None = None,
        **kwds: Any,
    ) -> None:
        super().__init__(**kwds)
        self._transmitters: dict[str, zmq.asyncio.Socket] = {}
        self._transmitter_uuids: dict[UUID, str] = {}
        self._cscp_lock = asyncio.Lock()

        self.register_chirp_callback("basecontroller_control", self._on_control_service)
        self.register_chirp_callback("basecontroller_heartbeat", self._on_heartbeat_service)

    def _on_control_service(self, event: CHIRPEvent, service: DiscoveredService) -> None:
        """Handle CONTROL service connect/disconnect."""
        if service.service_id != CHIRPServiceIdentifier.CONTROL:
            return
        if event == CHIRPEvent.SERVICE_CONNECTED:
            asyncio.ensure_future(self._setup_transmitter(service.host_id, service.addresses[0], service.port))
        elif event == CHIRPEvent.SERVICE_DISCONNECTED:
            self._cleanup_transmitter(service.host_id)

    def _on_heartbeat_service(self, event: CHIRPEvent, service: DiscoveredService) -> None:
        """Handle HEARTBEAT service connect/disconnect."""
        if service.service_id != CHIRPServiceIdentifier.HEARTBEAT:
            return
        if event == CHIRPEvent.SERVICE_CONNECTED:
            name = self._transmitter_uuids.get(service.host_id, f"Unknown-{str(service.host_id)[:8]}")
            self.register_heartbeat_host(service.host_id, service.addresses[0], service.port, name)
        elif event == CHIRPEvent.SERVICE_DISCONNECTED:
            self.unregister_heartbeat_host(service.host_id)

    async def _cscp_request(
        self,
        sock: zmq.asyncio.Socket,
        command: str,
        payload: Any = None,
    ) -> CSCP1Message:
        """Send a CSCP1 request and await the response.

        _cscp_lock serialises all REQ/REP exchanges so no two coroutines
        share a socket simultaneously.
        """
        request = CSCP1Message(self.name, (CSCP1Message.Type.REQUEST, command))
        if payload is not None:
            request.payload = payload
        async with self._cscp_lock:
            await sock.send_multipart(request.assemble().frames)
            response_frames = await sock.recv_multipart()
        return CSCP1Message.disassemble(response_frames)

    async def command(
        self,
        cmd: str,
        sat: str = "",
        satcls: str = "",
        payload: Any = None,
    ) -> str:
        """Send a CSCP command on the event loop."""
        key = f"{satcls}.{sat}".strip(".")
        sock = self._transmitters.get(key)
        if sock is None:
            return f"ERROR: No transmitter for {key}"
        try:
            result = await self._cscp_request(sock, cmd, payload)
            return f"{result.verb_msg}"
        except Exception as e:
            return f"ERROR: {e}"

    def _on_satellite_update(self, name: str, update_type: SatelliteUpdate) -> None:
        """Called on satellite connect/disconnect. Override in subclass."""

    async def _setup_transmitter(self, uuid: UUID, address: str, port: int) -> None:
        """Connect to a satellite CSCP port on the event loop."""
        if uuid in self._transmitter_uuids:
            return
        sock = self._async_ctx.socket(zmq.REQ)
        sock.connect(f"tcp://{address}:{port}")
        sock.setsockopt(zmq.LINGER, 2000)
        try:
            msg = await asyncio.wait_for(self._cscp_request(sock, "get_commands"), timeout=5.0)
        except Exception:
            sock.close()
            return
        canonical_name = msg.sender
        self._transmitters[canonical_name] = sock
        self._transmitter_uuids[uuid] = canonical_name
        self._on_satellite_update(canonical_name, SatelliteUpdate.ADDED)

    def _cleanup_transmitter(self, uuid: UUID) -> None:
        """Remove and close a CSCP socket on satellite departure."""
        canonical_name = self._transmitter_uuids.pop(uuid, None)
        if canonical_name is None:
            return
        sock = self._transmitters.pop(canonical_name, None)
        if sock is not None:
            sock.close()
            self._on_satellite_update(canonical_name, SatelliteUpdate.REMOVED)

    def shutdown(self) -> None:
        """Shut down all components."""
        for sock in self._transmitters.values():
            sock.close()
        self._hb_receiver.close()
        self._cmdp_pool.close()
        self._async_ctx.term()


class AsyncScriptableController(AsyncBaseController):
    """Async equivalent of ScriptableController."""

    def __init__(self, log_level: str = "INFO", **kwds: Any) -> None:
        setup_cli_logging(log_level)
        super().__init__(**kwds)
