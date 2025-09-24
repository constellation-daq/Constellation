"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2

CHIRPManger module provides classes for managing CHIRP multicasts within Constellation Satellites.
"""

import random
import threading
import time
from collections.abc import Callable
from functools import wraps
from typing import Any, ParamSpec, TypeVar
from uuid import UUID

from .base import BaseSatelliteFrame
from .chirp import (
    CHIRPBeaconTransmitter,
    CHIRPMessage,
    CHIRPMessageType,
    CHIRPServiceIdentifier,
)
from .network import get_interface_addresses

T = TypeVar("T")
B = TypeVar("B", bound=BaseSatelliteFrame)
P = ParamSpec("P")


def chirp_callback(
    request_service: CHIRPServiceIdentifier,
) -> Callable[[Callable[P, T]], Callable[P, T]]:
    """Mark a function as a callback for CHIRP service requests."""

    def decorator(func: Callable[P, T]) -> Callable[P, T]:
        @wraps(func)
        def wrapper(*args: P.args, **kwargs: P.kwargs) -> T:
            return func(*args, **kwargs)

        # mark function as chirp callback
        wrapper.chirp_callback = request_service  # type: ignore[attr-defined]
        return wrapper

    return decorator


class DiscoveredService:
    """Class to hold discovered service data."""

    def __init__(
        self,
        host_uuid: UUID,
        serviceid: CHIRPServiceIdentifier,
        address: str,
        port: int,
        alive: bool = True,
    ):
        """Initialize variables."""
        self.host_uuid = host_uuid
        self.address = address
        self.port = port
        self.serviceid = serviceid
        self.alive = alive

    def __eq__(self, other: object) -> bool:
        """Comparison operator for network-related properties."""
        if isinstance(other, DiscoveredService):
            return bool(self.host_uuid == other.host_uuid and self.serviceid == other.serviceid)
        return NotImplemented

    def __str__(self) -> str:
        """Pretty-print a string for this service."""
        s = "Host {} offering service {} on {}:{} is alive: {}"
        return s.format(self.host_uuid, self.serviceid, self.address, self.port, self.alive)


def get_chirp_callbacks(
    cls: object,
) -> dict[CHIRPServiceIdentifier, Callable[[B, DiscoveredService], None]]:
    """Loop over all class methods and return those marked as CHIRP callback."""
    res = {}
    for func in dir(cls):
        if isinstance(getattr(type(cls), func, None), property):
            # skip properties
            continue
        call = getattr(cls, func)
        if callable(call) and not func.startswith("__"):
            # regular method
            if hasattr(call, "chirp_callback"):
                res[getattr(call, "chirp_callback")] = call
    return res


class CHIRPManager(BaseSatelliteFrame):
    """Manages service discovery and sends multicast messages via the CHIRP protocol.

    Listening and reacting to CHIRP multicast messages is implemented in a dedicated
    thread that can be started after the class has been instantiated.

    Discovered services are added to an internal cache. Callback methods can be
    registered either by calling register_request() or by using the
    @chirp_callback() decorator. The callback will be added to the satellite's
    internal task queue once the corresponding service has been offered by other
    satellites via multicast.

    Offered services can be registered via register_offer() and are announced on
    incoming request messages or via emit_offers().

    """

    def __init__(
        self,
        name: str,
        group: str,
        interface: list[str] | None,
        **kwds: Any,
    ):
        """Initialize parameters.

        :param host: Satellite name this CHIRPManager represents
        :type host: str
        :param group: group the Satellite belongs to
        :type group: str
        """
        super().__init__(name=name, **kwds)
        self.group = group
        self._stop_emitting_chirp = threading.Event()

        self.log_chirp = self.get_logger("LINK")

        # Gather interface addresses
        interface_addresses = get_interface_addresses(interface)
        self.log_chirp.info("Using interfaces addresses %s", interface_addresses)

        self._beacon = CHIRPBeaconTransmitter(self.name, group, interface_addresses)

        # Offered and discovered services
        self._registered_services: dict[int, CHIRPServiceIdentifier] = {}
        self.discovered_services: list[DiscoveredService] = []
        self._chirp_thread = None
        self._chirp_callbacks: dict[CHIRPServiceIdentifier, Callable[[B, DiscoveredService], None]] = get_chirp_callbacks(
            self
        )

    def _add_com_thread(self) -> None:
        """Add the CHIRP manager thread to the communication thread pool."""
        super()._add_com_thread()
        self._com_thread_pool["chirp_manager"] = threading.Thread(target=self._run, daemon=True)
        self.log_chirp.debug("CHIRP manager thread prepared and added to the pool.")

    def get_discovered(self, serviceid: CHIRPServiceIdentifier) -> list[DiscoveredService]:
        """Return a list of already discovered services for a given identifier."""
        res = []
        for s in self.discovered_services:
            if s.serviceid == serviceid:
                res.append(s)
        return res

    def register_request(
        self,
        serviceid: CHIRPServiceIdentifier,
        callback: Callable[[B, DiscoveredService], None],
    ) -> None:
        """Register new callback for ServiceIdentifier."""
        if serviceid in self._chirp_callbacks:
            self.log_chirp.warning("Overwriting CHIRP callback")
        # FIXME the following assignment triggers an error with mypy
        self._chirp_callbacks[serviceid] = callback  # type: ignore[assignment]
        # make a callback if a service has already been discovered
        for known in self.get_discovered(serviceid):
            self.task_queue.put((callback, [known]))

    def register_offer(self, serviceid: CHIRPServiceIdentifier, port: int) -> None:
        """Register new offered service or overwrite existing service."""
        if port in self._registered_services:
            self.log_chirp.warning("Replacing service registration for port %d", port)
        self._registered_services[port] = serviceid

    def request(self, serviceid: CHIRPServiceIdentifier) -> None:
        """Request specific service.

        Should already have a registered callback via register_request(), or
        any incoming OFFERS will go unnoticed.

        """
        if serviceid not in self._chirp_callbacks:
            self.log_chirp.debug("Emitted REQUEST for %s does not have a registered callback", serviceid)
        self._beacon.emit(serviceid, CHIRPMessageType.REQUEST)

    def emit_offers(self, serviceid: CHIRPServiceIdentifier | None = None) -> None:
        """Emit messages all registered services matching `serviceid`.

        Specify None for all registered services.
        """
        for port, sid in self._registered_services.items():
            if not serviceid or serviceid == sid:
                self.log_chirp.debug("Sending service OFFER: %s for %s", port, sid)
                self._beacon.emit(sid, CHIRPMessageType.OFFER, port)

    def emit_requests(self) -> None:
        """Emit messages for all requests registered via register_request()."""
        for serviceid in self._chirp_callbacks:
            self.log_chirp.debug("Sending service REQUEST for %s", serviceid)
            self._beacon.emit(serviceid, CHIRPMessageType.REQUEST)

    def emit_depart(self) -> None:
        """Emit DEPART message for all registered services."""
        for port, sid in self._registered_services.items():
            self.log_chirp.debug("Sending service DEPART on %d for %s", port, sid)
            self._beacon.emit(sid, CHIRPMessageType.DEPART, port)

    def _discover_service(self, msg: CHIRPMessage) -> None:
        """Add a service to internal list and possibly queue a callback."""
        service = DiscoveredService(msg.host_uuid, msg.serviceid, msg.from_address, msg.port)
        already_discovered = False
        for discovered_service in self.discovered_services:
            if service == discovered_service:
                # Check if new port if service already discovered
                if service.port != discovered_service.port:
                    # Assume old host is dead
                    self.log_chirp.warning(
                        "%s has new port %d for %s service, assuming service has been replaced",
                        msg.host_uuid,
                        msg.port,
                        msg.serviceid.name,
                    )
                    # Remove old service
                    self.discovered_services.remove(discovered_service)
                    discovered_service.alive = False
                    self._call_callbacks(discovered_service)
                else:
                    self.log_chirp.debug(
                        "Service already discovered: %s on host %s:%s",
                        msg.serviceid.name,
                        msg.from_address,
                        msg.port,
                    )
                    already_discovered = True
                break
        if not already_discovered:
            # add service to internal list and queue callback
            self.log_chirp.debug(
                "Received new OFFER for service: %s on host %s:%s",
                msg.serviceid.name,
                msg.from_address,
                msg.port,
            )
            self.discovered_services.append(service)
            self._call_callbacks(service)

    def _depart_service(self, msg: CHIRPMessage) -> None:
        """Depart with a service."""
        try:
            service = DiscoveredService(msg.host_uuid, msg.serviceid, msg.from_address, msg.port)
            self.discovered_services.remove(service)
            self.log_chirp.debug(
                "Received depart for service %s on host %s: Removed.",
                msg.serviceid,
                msg.from_address,
            )
            # indicate that service is no longer with us
            service.alive = False
            self._call_callbacks(service)
        except ValueError:
            self.log_chirp.debug(
                "Received depart for service %s on host %s: Not in use.",
                msg.serviceid,
                msg.from_address,
            )

    def _call_callbacks(self, service: DiscoveredService) -> None:
        try:
            callback = self._chirp_callbacks[service.serviceid]
            self.task_queue.put((callback, [service]))
        except KeyError:
            self.log_chirp.debug("No callback for service %s set up.", service.serviceid)

    def _run(self) -> None:
        """Start listening in on incoming multicast messages"""
        # assert for mypy static type analysis
        assert isinstance(self._com_thread_evt, threading.Event), "CHIRPManager thread Event no set up"

        while not self._com_thread_evt.is_set():
            msg = self._beacon.listen()
            if not msg:
                continue

            self.log_chirp.trace(
                "Received CHIRP %s for %s from '%s'",
                msg.msgtype.name,
                msg.serviceid.name,
                msg.from_address,
            )

            # Check Message Type
            if msg.msgtype == CHIRPMessageType.REQUEST:
                # wait a short moment to spread out responses somewhat
                time.sleep(random.random() / 5.0)
                self.emit_offers(serviceid=msg.serviceid)
                continue

            if msg.msgtype == CHIRPMessageType.OFFER:
                self._discover_service(msg)
                continue

            if msg.msgtype == CHIRPMessageType.DEPART and msg.port != 0:
                self._depart_service(msg)
                continue
        # shutdown
        self.log_chirp.debug("CHIRPManager thread shutting down.")
        self.emit_depart()
        # it can take a moment for the network buffers to be flushed
        time.sleep(0.5)
        self._beacon.close()
