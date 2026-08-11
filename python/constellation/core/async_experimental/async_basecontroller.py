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
from constellation.core.protocol.cscp1 import TransitionCommand


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
        # In-flight _setup_transmitter tasks keyed by host_id, cancelled
        # on SERVICE_DISCONNECTED to prevent stale sockets.
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

            def _on_setup_done(t: asyncio.Task, host_id: UUID = service.host_id) -> None:
                self._pending_setups.pop(host_id, None)
                if not t.cancelled() and t.exception() is not None:
                    self.log.exception("Setup failed for %s", host_id, exc_info=t.exception())

            task.add_done_callback(_on_setup_done)
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

    def _resolve_targets(self, satcls: str, sat: str) -> list[str]:
        """Resolve command targets.

        If both are empty, targets all connected satellites.
        If only satcls is given, targets all satellites of that type.
        If both are given, targets a single satellite.
        """
        if not satcls and not sat:
            return list(self._transmitters.keys())
        if satcls and not sat:
            prefix = f"{satcls}."
            return [k for k in self._transmitters if k.startswith(prefix)]
        canonical_name = f"{satcls}.{sat}".strip(".")
        if canonical_name in self._transmitters:
            return [canonical_name]
        return []

    async def command(
        self,
        cmd: str,
        sat: str = "",
        satcls: str = "",
        payload: Any = None,
        timeout: float = 10.0,
    ) -> CSCP1Message:
        """Send a CSCP command on the event loop and return the raw response.

        Targets can be a single satellite, all satellites of a type, or
        all connected satellites depending on which of sat and satcls are
        provided. When targeting multiple satellites, returns the last
        response.

        A per satellite lock serialises REQ/REP exchanges so concurrent
        commands to different satellites proceed in parallel. If a satellite
        fails to respond within timeout seconds, its socket is torn down
        and a TimeoutError is raised.
        """
        targets = self._resolve_targets(satcls, sat)
        if not targets:
            canonical_name = f"{satcls}.{sat}".strip(".")
            raise RuntimeError(f"No transmitter for {canonical_name or 'any satellite'}")

        is_transition = cmd in [t.name for t in TransitionCommand]

        last_response: CSCP1Message | None = None
        for canonical_name in targets:
            sock = self._transmitters.get(canonical_name)
            if sock is None:
                continue
            lock = self._cscp_locks.setdefault(canonical_name, asyncio.Lock())
            processed = self._preprocess_payload(payload, canonical_name, cmd)

            # Mark heartbeat state as outdated for transition commands
            hb = self._hb_receiver._states.get(
                next((u for u, n in self._transmitter_uuids.items() if n == canonical_name), None)
            )
            if is_transition and hb is not None:
                hb.outdated = True

            try:
                last_response = await asyncio.wait_for(
                    self._cscp_request(sock, cmd, processed, lock),
                    timeout=timeout,
                )
            except TimeoutError:
                if hb is not None:
                    hb.outdated = False
                self._cleanup_transmitter(canonical_name)
                raise

            # Reset outdated flag if the command was not accepted
            if hb is not None and is_transition:
                if last_response.verb_type != CSCP1Message.Type.SUCCESS:
                    hb.outdated = False

        return last_response  # type: ignore[return-value]

    def _preprocess_payload(self, payload: Any, canonical_name: str, cmd: str) -> Any:
        """Pre-process payload for initialize and reconfigure commands.

        Accepts a plain dict, Configuration, or ControllerConfiguration
        and returns a plain dict suitable for CSCP transmission.
        """
        if cmd in ("initialize", "reconfigure"):
            if isinstance(payload, ControllerConfiguration):
                payload = payload.get_satellite_configuration(canonical_name)
            elif isinstance(payload, dict):
                payload = Configuration(payload)
            elif not isinstance(payload, Configuration):
                raise RuntimeError("Payload needs to be a dictionary, configuration or controller configuration")
            return payload._dictionary
        return payload

    def get_available_cscp_commands(self, canonical_name: str) -> dict[str, str]:
        """Return advertised commands for a satellite, or {} if unknown."""
        return self._satellite_commands.get(canonical_name, {})

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
            # Cancelled via _pending_setups on SERVICE_DISCONNECTED
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

    def _cleanup_transmitter(self, satellite: UUID | str) -> None:
        """Remove and close a CSCP socket by UUID or canonical name."""
        if isinstance(satellite, UUID):
            canonical_name = self._transmitter_uuids.pop(satellite, None)
            if canonical_name is None:
                return
        else:
            canonical_name = satellite
            uuid = next(
                (u for u, name in self._transmitter_uuids.items() if name == canonical_name),
                None,
            )
            if uuid is not None:
                self._transmitter_uuids.pop(uuid, None)
        sock = self._transmitters.pop(canonical_name, None)
        self._cscp_locks.pop(canonical_name, None)
        self._satellite_commands.pop(canonical_name, None)
        if sock is not None:
            sock.close()
            self._on_satellite_update(canonical_name, SatelliteUpdate.REMOVED)

    async def shutdown(self) -> None:
        """Shut down all components.

        Awaits cancelled setup tasks so their socket cleanup runs
        before ctx.term() blocks on unclosed sockets.
        """
        pending = [t for t in self._pending_setups.values() if not t.done()]
        for task in pending:
            task.cancel()
        if pending:
            await asyncio.gather(*pending, return_exceptions=True)
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
