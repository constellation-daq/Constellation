"""
SPDX-FileCopyrightText: 2026 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2
"""

import asyncio
import socket
import sys
from collections.abc import Callable
from dataclasses import dataclass, field
from enum import Enum, auto
from uuid import UUID

from constellation.core.chirp import (
    CHIRP_MULTICAST_ADDRESS,
    CHIRP_PORT,
    CHIRPMessage,
    CHIRPMessageType,
    CHIRPServiceIdentifier,
    get_uuid,
)
from constellation.core.multicast import MULTICAST_TTL


class CHIRPEvent(Enum):
    """Event type for CHIRP service callbacks."""

    SERVICE_CONNECTED = auto()
    SERVICE_DISCONNECTED = auto()


@dataclass(eq=False)
class DiscoveredService:
    """A service discovered via CHIRP.

    addresses is a list because a host may be reachable on multiple network
    interfaces. Additional addresses are appended as OFFERs arrive from the
    same host on different interfaces.
    """

    group_id: UUID
    host_id: UUID
    service_id: CHIRPServiceIdentifier
    port: int
    addresses: list[str] = field(default_factory=list)

    def __eq__(self, other: object) -> bool:
        """Equality on host_id + service_id only, matching chirpmanager.py."""
        if isinstance(other, DiscoveredService):
            return self.host_id == other.host_id and self.service_id == other.service_id
        return NotImplemented


class AsyncMulticastSocket(asyncio.DatagramProtocol):
    """Async-compatible multicast socket and DatagramProtocol.

    Owns the raw socket setup and the asyncio transport layer.
    Incoming datagrams are forwarded to datagram_callback when set.

    Send sockets remain synchronous since UDP sendto writes to the
    kernel buffer without blocking.
    """

    def __init__(
        self,
        interface_addresses: list[str],
        multicast_address: str,
        multicast_port: int,
    ) -> None:
        self._multicast_endpoint = (multicast_address, multicast_port)
        self._datagram_callback: Callable[[bytes, tuple[str, int]], None] | None = None
        self._transport: asyncio.DatagramTransport | None = None
        self._send_sockets: list[socket.socket] = []
        for interface_address in interface_addresses:
            sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, MULTICAST_TTL)
            sock.setsockopt(
                socket.IPPROTO_IP,
                socket.IP_MULTICAST_LOOP,
                0 if interface_address != "127.0.0.1" else 1,
            )
            sock.setsockopt(
                socket.IPPROTO_IP,
                socket.IP_MULTICAST_IF,
                socket.inet_aton(interface_address),
            )
            self._send_sockets.append(sock)

        self._recv_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
        self._recv_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        if sys.platform == "darwin":
            self._recv_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
        self._recv_socket.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, MULTICAST_TTL)
        self._recv_socket.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_LOOP, 1)
        self._recv_socket.bind(("0.0.0.0", multicast_port))
        for interface_address in interface_addresses:
            ip_mreq = socket.inet_aton(multicast_address) + socket.inet_aton(interface_address)
            self._recv_socket.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, ip_mreq)
        self._recv_socket.setblocking(False)

    @property
    def datagram_callback(self) -> Callable[[bytes, tuple[str, int]], None] | None:
        """Callback invoked by datagram_received for each incoming packet."""
        return self._datagram_callback

    @datagram_callback.setter
    def datagram_callback(self, callback: Callable[[bytes, tuple[str, int]], None] | None) -> None:
        self._datagram_callback = callback

    def connection_made(self, transport: asyncio.DatagramTransport) -> None:
        self._transport = transport

    def datagram_received(self, data: bytes, addr: tuple[str, int]) -> None:
        if self._datagram_callback is not None:
            self._datagram_callback(data, addr)

    async def start(self, loop: asyncio.AbstractEventLoop) -> None:
        """Hand the receive socket to the event loop."""
        await loop.create_datagram_endpoint(
            lambda: self,
            sock=self._recv_socket,
        )

    def send(self, data: bytes) -> None:
        """Send data on all interfaces."""
        for sock in self._send_sockets:
            sock.sendto(data, self._multicast_endpoint)

    def close(self) -> None:
        """Close all sockets and the transport."""
        for sock in self._send_sockets:
            sock.close()
        if self._transport is not None:
            self._transport.close()


class AsyncCHIRPListener:
    """Async CHIRP listener.

    Owns an AsyncMulticastSocket, decodes incoming CHIRP messages,
    maintains a list of discovered services (mirroring CHIRPManager logic),
    and dispatches to registered callbacks.

    Callbacks receive (CHIRPEvent, DiscoveredService).
    REQUEST messages are forwarded to request_callback if set.
    """

    def __init__(
        self,
        name: str,
        group: str,
        interface_addresses: list[str],
    ) -> None:
        self._host_uuid = get_uuid(name)
        self._group_uuid = get_uuid(group)
        self._socket = AsyncMulticastSocket(
            interface_addresses,
            CHIRP_MULTICAST_ADDRESS,
            CHIRP_PORT,
        )
        self._socket.datagram_callback = self._on_datagram_received
        self._callbacks: dict[str, Callable[[CHIRPEvent, DiscoveredService], None]] = {}
        self._request_callback: Callable[[CHIRPServiceIdentifier], None] | None = None
        self._discovered_services: list[DiscoveredService] = []

    def register_callback(
        self,
        callback_id: str,
        callback_func: Callable[[CHIRPEvent, DiscoveredService], None],
    ) -> None:
        """Register a callback for CHIRP service events."""
        self._callbacks[callback_id] = callback_func

    def unregister_callback(self, callback_id: str) -> None:
        """Remove a previously registered callback."""
        self._callbacks.pop(callback_id, None)

    def send_chirp_message(self, message: CHIRPMessage) -> None:
        """Send a CHIRP message on all interfaces."""
        self._socket.send(message.pack())

    def get_discovered(self, service_id: CHIRPServiceIdentifier) -> list[DiscoveredService]:
        """Return all discovered services matching service_id."""
        return [s for s in self._discovered_services if s.service_id == service_id]

    @property
    def request_callback(self) -> Callable[[CHIRPServiceIdentifier], None] | None:
        """Callback invoked when a CHIRP REQUEST is received."""
        return self._request_callback

    @request_callback.setter
    def request_callback(
        self,
        callback: Callable[[CHIRPServiceIdentifier], None] | None,
    ) -> None:
        self._request_callback = callback

    async def start(self, loop: asyncio.AbstractEventLoop) -> None:
        """Start the multicast socket on the event loop."""
        await self._socket.start(loop)

    def close(self) -> None:
        """Close the socket and transport."""
        self._socket.close()

    def _on_datagram_received(self, data: bytes, addr: tuple[str, int]) -> None:
        """Decode and dispatch an incoming CHIRP datagram."""
        msg = CHIRPMessage()
        try:
            msg.unpack(data)
        except Exception:
            return

        if msg.host_uuid == self._host_uuid:
            return
        if msg.group_uuid != self._group_uuid:
            return

        msg.from_address = addr[0]

        if msg.msgtype == CHIRPMessageType.REQUEST:
            if self._request_callback is not None:
                self._request_callback(msg.serviceid)
        elif msg.msgtype == CHIRPMessageType.OFFER:
            self._discover_service(msg, addr[0])
        elif msg.msgtype == CHIRPMessageType.DEPART and msg.port != 0:
            self._depart_service(msg)

    def _discover_service(self, msg: CHIRPMessage, from_address: str) -> None:
        """Mirror CHIRPManager._discover_service logic."""
        already_discovered = False
        for svc in list(self._discovered_services):
            if svc.host_id == msg.host_uuid and svc.service_id == msg.serviceid:
                if svc.port != msg.port:
                    # Port changed (assume old service is gone)
                    self._discovered_services.remove(svc)
                    for cb in self._callbacks.values():
                        cb(CHIRPEvent.SERVICE_DISCONNECTED, svc)
                else:
                    # Same service, same port (accumulate address if new)
                    if from_address not in svc.addresses:
                        svc.addresses.append(from_address)
                    already_discovered = True
                break

        if not already_discovered:
            svc = DiscoveredService(
                group_id=msg.group_uuid,
                host_id=msg.host_uuid,
                service_id=msg.serviceid,
                port=msg.port,
                addresses=[from_address],
            )
            self._discovered_services.append(svc)
            for cb in self._callbacks.values():
                cb(CHIRPEvent.SERVICE_CONNECTED, svc)

    def _depart_service(self, msg: CHIRPMessage) -> None:
        """Mirror CHIRPManager._depart_service logic."""
        for svc in list(self._discovered_services):
            if svc.host_id == msg.host_uuid and svc.service_id == msg.serviceid:
                self._discovered_services.remove(svc)
                for cb in self._callbacks.values():
                    cb(CHIRPEvent.SERVICE_DISCONNECTED, svc)
                break


class AsyncCHIRPManager(AsyncCHIRPListener):
    """Async CHIRP manager with service registry.

    Inherits AsyncCHIRPListener and adds the ability to register
    services to offer and to send service requests.
    """

    def __init__(
        self,
        name: str,
        group: str,
        interface_addresses: list[str],
    ) -> None:
        super().__init__(name, group, interface_addresses)
        self._registered_services: dict[CHIRPServiceIdentifier, int] = {}
        self.request_callback = self._handle_request

    def request(self, service_id: CHIRPServiceIdentifier) -> None:
        """Send a CHIRP REQUEST for a specific service type."""
        msg = CHIRPMessage(
            CHIRPMessageType.REQUEST,
            self._group_uuid,
            self._host_uuid,
            service_id,
            0,
        )
        self.send_chirp_message(msg)

    def register_service(self, service_id: CHIRPServiceIdentifier, port: int) -> None:
        """Register a service to offer via CHIRP.

        If the service was already registered on a different port, sends DEPART
        for the old port first. Sends OFFER immediately upon registration.
        """
        if service_id in self._registered_services:
            old_port = self._registered_services[service_id]
            depart = CHIRPMessage(
                CHIRPMessageType.DEPART,
                self._group_uuid,
                self._host_uuid,
                service_id,
                old_port,
            )
            self.send_chirp_message(depart)
        self._registered_services[service_id] = port
        offer = CHIRPMessage(
            CHIRPMessageType.OFFER,
            self._group_uuid,
            self._host_uuid,
            service_id,
            port,
        )
        self.send_chirp_message(offer)

    def unregister_service(self, service_id: CHIRPServiceIdentifier) -> None:
        """Unregister a service and send DEPART."""
        port = self._registered_services.pop(service_id, None)
        if port is not None:
            msg = CHIRPMessage(
                CHIRPMessageType.DEPART,
                self._group_uuid,
                self._host_uuid,
                service_id,
                port,
            )
            self.send_chirp_message(msg)

    def emit_offers(self) -> None:
        """Send OFFER for all registered services."""
        for service_id, port in self._registered_services.items():
            msg = CHIRPMessage(
                CHIRPMessageType.OFFER,
                self._group_uuid,
                self._host_uuid,
                service_id,
                port,
            )
            self.send_chirp_message(msg)

    def _handle_request(self, service_id: CHIRPServiceIdentifier) -> None:
        """Respond to incoming CHIRP requests with matching offers."""
        if service_id in self._registered_services:
            port = self._registered_services[service_id]
            msg = CHIRPMessage(
                CHIRPMessageType.OFFER,
                self._group_uuid,
                self._host_uuid,
                service_id,
                port,
            )
            self.send_chirp_message(msg)
