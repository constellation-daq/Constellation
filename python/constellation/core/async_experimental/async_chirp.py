import asyncio
import socket
import sys
from collections.abc import Callable
from dataclasses import dataclass
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


@dataclass
class DiscoveredService:
    """A service discovered via CHIRP."""

    group_id: UUID
    host_id: UUID
    service_id: CHIRPServiceIdentifier
    port: int
    address: str


class AsyncMulticastSocket:
    """Async-compatible multicast socket.

    Mirrors MulticastSocket but sets the receive socket non-blocking
    so it can be handed to asyncio via create_datagram_endpoint.
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
            ip_mreq = (
                socket.inet_aton(multicast_address)
                + socket.inet_aton(interface_address)
            )
            self._recv_socket.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, ip_mreq)
        self._recv_socket.setblocking(False)

    def send(self, data: bytes) -> None:
        """Send data on all interfaces."""
        for sock in self._send_sockets:
            sock.sendto(data, self._multicast_endpoint)

    @property
    def recv_socket(self) -> socket.socket:
        """The configured non-blocking receive socket."""
        return self._recv_socket

    def close(self) -> None:
        """Close all sockets."""
        for sock in self._send_sockets:
            sock.close()
        self._recv_socket.close()


class AsyncCHIRPListener(asyncio.DatagramProtocol):
    """Async CHIRP listener using asyncio.DatagramProtocol.

    Owns an AsyncMulticastSocket, decodes incoming CHIRP messages,
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
        self._callbacks: dict[str, Callable[[CHIRPEvent, DiscoveredService], None]] = {}
        self._request_callback: Callable[[CHIRPServiceIdentifier], None] | None = None
        self._transport: asyncio.DatagramTransport | None = None

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

    def connection_made(self, transport: asyncio.DatagramTransport) -> None:
        self._transport = transport

    def datagram_received(self, data: bytes, addr: tuple[str, int]) -> None:
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
            return

        service = DiscoveredService(
            group_id=msg.group_uuid,
            host_id=msg.host_uuid,
            service_id=msg.serviceid,
            port=msg.port,
            address=addr[0],
        )

        if msg.msgtype == CHIRPMessageType.OFFER:
            event = CHIRPEvent.SERVICE_CONNECTED
        elif msg.msgtype == CHIRPMessageType.DEPART and msg.port != 0:
            event = CHIRPEvent.SERVICE_DISCONNECTED
        else:
            return

        for callback in self._callbacks.values():
            callback(event, service)

    async def start(self, loop: asyncio.AbstractEventLoop) -> None:
        """Hand the receive socket to the event loop."""
        await loop.create_datagram_endpoint(
            lambda: self,
            sock=self._socket.recv_socket,
        )

    def close(self) -> None:
        """Close the socket and transport."""
        self._socket.close()
        if self._transport is not None:
            self._transport.close()


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
        """Register a service to offer via CHIRP."""
        self._registered_services[service_id] = port

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
