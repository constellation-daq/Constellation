import asyncio
import socket
import sys
from collections.abc import Callable
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


class AsyncCHIRPProtocol(asyncio.DatagramProtocol):
    """Async CHIRP discovery using asyncio.DatagramProtocol.

    The receive socket is handed to the event loop via create_datagram_endpoint
    so datagram_received runs on the event loop. Send sockets use synchronous
    sendto, which is acceptable for UDP since it writes to the kernel buffer
    without blocking.

    on_offer and on_depart are called directly from datagram_received and must
    not block. Schedule any blocking work as a task from the caller.
    """

    def __init__(
        self,
        name: str,
        group: str,
        interface_addresses: list[str],
        on_offer: Callable[[UUID, str, int, CHIRPServiceIdentifier], None],
        on_depart: Callable[[UUID, CHIRPServiceIdentifier], None],
    ) -> None:
        self._host_uuid = get_uuid(name)
        self._group_uuid = get_uuid(group)
        self._on_offer = on_offer
        self._on_depart = on_depart
        self._multicast_endpoint = (CHIRP_MULTICAST_ADDRESS, CHIRP_PORT)
        self._send_sockets = self._create_send_sockets(interface_addresses)
        self._transport: asyncio.DatagramTransport | None = None

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
            return

        if msg.msgtype == CHIRPMessageType.OFFER:
            self._on_offer(msg.host_uuid, msg.from_address, msg.port, msg.serviceid)
        elif msg.msgtype == CHIRPMessageType.DEPART and msg.port != 0:
            self._on_depart(msg.host_uuid, msg.serviceid)

    def emit(self, serviceid: CHIRPServiceIdentifier, msgtype: CHIRPMessageType, port: int = 0) -> None:
        """Send a CHIRP message on all interfaces."""
        msg = CHIRPMessage(msgtype, self._group_uuid, self._host_uuid, serviceid, port)
        packed = msg.pack()
        for sock in self._send_sockets:
            sock.sendto(packed, self._multicast_endpoint)

    def close(self) -> None:
        """Close send sockets and the transport."""
        for sock in self._send_sockets:
            sock.close()
        if self._transport is not None:
            self._transport.close()

    @staticmethod
    def create_recv_socket(interface_addresses: list[str]) -> socket.socket:
        """Create and configure the multicast receive socket for asyncio."""
        recv_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
        recv_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        if sys.platform == "darwin":
            recv_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
        recv_socket.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, MULTICAST_TTL)
        recv_socket.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_LOOP, 1)
        recv_socket.bind(("0.0.0.0", CHIRP_PORT))
        for interface_address in interface_addresses:
            ip_mreq = socket.inet_aton(CHIRP_MULTICAST_ADDRESS) + socket.inet_aton(interface_address)
            recv_socket.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, ip_mreq)
        recv_socket.setblocking(False)
        return recv_socket

    @staticmethod
    def _create_send_sockets(interface_addresses: list[str]) -> list[socket.socket]:
        send_sockets = []
        for interface_address in interface_addresses:
            sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, MULTICAST_TTL)
            sock.setsockopt(
                socket.IPPROTO_IP,
                socket.IP_MULTICAST_LOOP,
                0 if interface_address != "127.0.0.1" else 1,
            )
            sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_IF, socket.inet_aton(interface_address))
            send_sockets.append(sock)
        return send_sockets
