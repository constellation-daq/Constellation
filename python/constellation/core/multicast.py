"""
SPDX-FileCopyrightText: 2025 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2

This module provides a multicast socket.
"""

import socket
import sys
from dataclasses import dataclass

MESSAGE_BUFFER = 1024
MULTICAST_TTL = 8


@dataclass
class MulticastMessage:
    content: bytes
    address: str


class MulticastSocket:
    def __init__(self, interface_addresses: list[str], multicast_address: str, multicast_port: int) -> None:
        self._multicast_endpoint = (multicast_address, multicast_port)

        # Receive endpoint using any address and multicast port
        recv_endpoint = ("0.0.0.0", multicast_port)

        # Create send sockets
        self._send_sockets = list[socket.socket]()
        for interface_address in interface_addresses:
            # Create socket for UDP
            send_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)

            # Ensure socket can be bound by other programs
            send_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

            # Set Multicast TTL (aka network hops)
            send_socket.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, MULTICAST_TTL)

            # Disable loopback since loopback interface is added explicitly
            send_socket.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_LOOP, 0 if interface_address != "127.0.0.1" else 1)

            # Set interface address
            send_socket.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_IF, socket.inet_aton(interface_address))

            self._send_sockets.append(send_socket)

        # Create receive socket
        self._recv_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)

        # Ensure socket can be bound by other programs
        self._recv_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

        # Set SO_REUSEPORT on MacOS to mirror C++ asio behavior
        if sys.platform == "darwin":
            self._recv_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)

        # Set Multicast TTL (aka network hops)
        self._recv_socket.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, MULTICAST_TTL)

        # Enable loopback
        self._recv_socket.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_LOOP, 1)

        # Bind socket
        self._recv_socket.bind(recv_endpoint)

        # Join multicast group on each interface
        for interface_address in interface_addresses:
            ip_mreq = socket.inet_aton(multicast_address) + socket.inet_aton(interface_address)
            self._recv_socket.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, ip_mreq)

        # Set receive timeout
        self._recv_socket.settimeout(0.05)

    def sendMessage(self, message: bytes) -> None:
        for send_socket in self._send_sockets:
            send_socket.sendto(message, self._multicast_endpoint)

    def recvMessage(self) -> MulticastMessage | None:
        try:
            bytes, sender_address = self._recv_socket.recvfrom(MESSAGE_BUFFER)
            return MulticastMessage(bytes, sender_address[0])
        except TimeoutError:
            pass
        return None

    def close(self) -> None:
        for send_socket in self._send_sockets:
            send_socket.close()
        self._recv_socket.close()
