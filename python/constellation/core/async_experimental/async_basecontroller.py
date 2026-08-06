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
from constellation.core.configuration import Configuration
from constellation.core.controller import SatelliteUpdate
from constellation.core.controller_configuration import ControllerConfiguration
from constellation.core.logging import setup_cli_logging
from constellation.core.message.cscp1 import CSCP1Message


class AsyncBaseController(AsyncMonitoringListener, AsyncHeartbeatChecker):
    """Async equivalent of BaseController.

    CSCP commands use zmq.asyncio for direct send/recv, eliminating the
    dedicated executor and synchronous context. All ZMQ sockets including
    CSCP REQ sockets share _async_ctx from BaseSatelliteFrame. Discovery
    and heartbeat run as coroutines.
    """

    def __init__(
        self,
        **kwds: Any,
    ) -> None:
        super().__init__(**kwds)
        self._transmitters: dict[str, zmq.asyncio.Socket] = {}
        self._transmitter_uuids: dict[UUID, str] = {}
        self._cscp_locks: dict[str, asyncio.Lock] = {}
        self._satellite_commands: dict[str, dict[str, str]] = {}
        # In-flight _setup_transmitter tasks keyed by host_id, allowing
        # SERVICE_DISCONNECTED to cancel mid-setup tasks safely.
        self._pending_setups: dict[UUID, asyncio.Task] = {}

        self.register_chirp_callback("basecontroller_control", self._on_control_service)
        self.register_chirp_callback("basecontroller_heartbeat", self._on_heartbeat_service)

    async def run(self, stop: asyncio.Event) -> None:
        """Register (once) and run every async communication task until stop is set."""
        if not self._com_task_factories:
            self._add_com_task()
        await self._start_com_tasks(stop)

    def _on_control_service(self, event: CHIRPEvent, service: DiscoveredService) -> None:
        """Handle CONTROL service connect/disconnect."""
        if service.service_id != CHIRPServiceIdentifier.CONTROL:
            return
        if event == CHIRPEvent.SERVICE_CONNECTED:
            task = asyncio.ensure_future(self._setup_transmitter(service.host_id, service.addresses[0], service.port))
            self._pending_setups[service.host_id] = task
            task.add_done_callback(lambda _t, host_id=service.host_id: self._pending_setups.pop(host_id, None))
        elif event == CHIRPEvent.SERVICE_DISCONNECTED:
            pending = self._pending_setups.pop(service.host_id, None)
            if pending is not None and not pending.done():
                pending.cancel()
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
        payload: Any,
        lock: asyncio.Lock,
    ) -> CSCP1Message:
        """Send a CSCP1 request and await the response.

        The caller-supplied lock serialises REQ/REP exchanges per satellite
        socket, allowing concurrent commands to different satellites.
        """
        request = CSCP1Message(self.name, (CSCP1Message.Type.REQUEST, command))
        if payload is not None:
            request.payload = payload
        async with lock:
            await sock.send_multipart(request.assemble().frames)
            response_frames = await sock.recv_multipart()
        return CSCP1Message.disassemble(response_frames)

    async def command(
        self,
        cmd: str,
        sat: str = "",
        satcls: str = "",
        payload: Any = None,
        timeout: float = 10.0,
    ) -> CSCP1Message:
        """Send a CSCP command on the event loop and return the raw response.

        Callers should check verb_type (SUCCESS, INVALID, etc.) and inspect
        payload structurally rather than pattern matching on verb_msg strings,
        which are human readable and may change between versions.

        A per satellite lock serialises REQ/REP exchanges so concurrent
        commands to different satellites proceed in parallel. If a satellite
        fails to respond within timeout seconds, its socket is torn down
        (a ZMQ REQ socket cannot be reused after an abandoned recv) and a
        TimeoutError is raised.

        Raises ValueError if both sat and satcls are empty.
        Raises RuntimeError if no transmitter is connected for the target.
        Raises TimeoutError if the satellite does not respond in time.
        """
        key = f"{satcls}.{sat}".strip(".")
        if not key:
            raise ValueError("satcls and sat must be provided (e.g. satcls='PyRandomTransmitter', sat='Sat1')")
        sock = self._transmitters.get(key)
        if sock is None:
            raise RuntimeError(f"No transmitter for {key}")
        lock = self._cscp_locks.setdefault(key, asyncio.Lock())
        payload = self._preprocess_payload(payload, key, cmd)
        try:
            return await asyncio.wait_for(
                self._cscp_request(sock, cmd, payload, lock),
                timeout=timeout,
            )
        except TimeoutError:
            self._teardown_transmitter(key)
            raise

    def _preprocess_payload(self, payload: Any, key: str, cmd: str) -> Any:
        """Pre-process payload for initialize and reconfigure commands.

        Accepts a plain dict, Configuration, or ControllerConfiguration
        and returns a plain dict suitable for CSCP transmission.
        """
        if cmd in ("initialize", "reconfigure"):
            if isinstance(payload, ControllerConfiguration):
                payload = payload.get_satellite_configuration(key)
            elif payload is None or isinstance(payload, dict):
                payload = Configuration(payload or {})
            elif not isinstance(payload, Configuration):
                raise RuntimeError("Payload needs to be a dictionary, configuration or controller configuration")
            return payload._dictionary
        return payload

    def get_cached_commands(self, key: str) -> dict[str, str]:
        """Return cached get_commands payload for a satellite, or {} if unknown."""
        return self._satellite_commands.get(key, {})

    def _on_satellite_update(self, name: str, update_type: SatelliteUpdate) -> None:
        """Called on satellite connect/disconnect. Override in subclass."""

    def _on_heartbeat_stale(self, uuid: UUID) -> None:
        """Discard all CHIRP services for a stale host.

        Matches Controller.cpp controller_loop which calls
        forgetDiscoveredServices when heartbeat lives are exhausted.
        """
        self.forget_host(uuid)

    async def _setup_transmitter(self, uuid: UUID, address: str, port: int) -> None:
        """Connect to a satellite CSCP port on the event loop."""
        if uuid in self._transmitter_uuids:
            return
        sock = self._async_ctx.socket(zmq.REQ)
        sock.connect(f"tcp://{address}:{port}")
        sock.setsockopt(zmq.LINGER, 2000)
        try:
            msg = await asyncio.wait_for(
                self._cscp_request(sock, "get_commands", None, asyncio.Lock()),
                timeout=5.0,
            )
        except asyncio.CancelledError:
            # Cancelled by SERVICE_DISCONNECTED during setup; close and propagate.
            sock.close()
            raise
        except Exception:
            sock.close()
            return
        canonical_name = msg.sender
        self._transmitters[canonical_name] = sock
        self._transmitter_uuids[uuid] = canonical_name
        self._cscp_locks[canonical_name] = asyncio.Lock()
        self._satellite_commands[canonical_name] = msg.payload if isinstance(msg.payload, dict) else {}
        self._on_satellite_update(canonical_name, SatelliteUpdate.ADDED)

    def _cleanup_transmitter(self, uuid: UUID) -> None:
        """Remove and close a CSCP socket on satellite departure."""
        canonical_name = self._transmitter_uuids.pop(uuid, None)
        if canonical_name is None:
            return
        sock = self._transmitters.pop(canonical_name, None)
        self._cscp_locks.pop(canonical_name, None)
        self._satellite_commands.pop(canonical_name, None)
        if sock is not None:
            sock.close()
            self._on_satellite_update(canonical_name, SatelliteUpdate.REMOVED)

    def _teardown_transmitter(self, key: str) -> None:
        """Tear down a transmitter by canonical name after a timeout.

        A ZMQ REQ socket cannot be reused once a reply is abandoned
        mid-flight, so we must close it entirely. The satellite will
        need to be rediscovered via CHIRP to reconnect.
        """
        sock = self._transmitters.pop(key, None)
        self._cscp_locks.pop(key, None)
        self._satellite_commands.pop(key, None)
        uuid = next(
            (u for u, name in self._transmitter_uuids.items() if name == key),
            None,
        )
        if uuid is not None:
            self._transmitter_uuids.pop(uuid, None)
        if sock is not None:
            sock.close()
            self._on_satellite_update(key, SatelliteUpdate.REMOVED)

    def shutdown(self) -> None:
        """Shut down all components."""
        for task in list(self._pending_setups.values()):
            if not task.done():
                task.cancel()
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
