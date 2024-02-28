"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0

BroadcastManger module provides classes for managing CHIRP broadcasts within
Constellation Satellites.
"""

import socket
from uuid import UUID, uuid5, NAMESPACE_DNS
from enum import Enum
import msgpack


CHIRP_PORT = 7123
CHIRP_HEADER = "CHIRP%x01"


class CHIRPServiceIdentifier(Enum):
    """Identifies the type of service.

    The CONTROL service identifier indicates a CSCP (Constellation Satellite
    Control Protocol) service.

    The HEARTBEAT service identifier indicates a CHBP (Constellation Heartbeat
    Broadcasting Protocol) service.

    The MONITORING service identifier indicates a CMDP (Constellation Monitoring
    Distribution Protocol) service.

    The DATA service identifier indicates a CDTP (Constellation Data
    Transmission Protocol) service.

    """

    CONTROL = 0x1
    HEARTBEAT = 0x2
    MONITORING = 0x3
    DATA = 0x4


class CHIRPMessageType(Enum):
    """Identifies the type of message sent or received via the CHIRP protocol.

    See docs/protocols/chirp.md for details.

    REQUEST: A message with REQUEST type indicates that CHIRP hosts should reply
    with an OFFER

    OFFER: A message with OFFER type indicates that service is available

    DEPART: A message with DEPART type indicates that a service is no longer
    available

    """

    REQUEST = 0x1
    OFFER = 0x2
    DEPART = 0x3


class CHIRPMessage:
    """Class to hold a CHIRP message."""

    def __init__(
        self,
        msgtype: CHIRPMessageType = None,
        group_uuid: UUID = None,
        host_uuid: UUID = None,
        serviceid: CHIRPServiceIdentifier = None,
        port: int = 0,
    ):
        """Initialize attributes."""
        self.msgtype = msgtype
        self.group_uuid = group_uuid
        self.host_uuid = host_uuid
        self.serviceid = serviceid
        self.port = port
        self.from_address = None

    def pack(self):
        """Serialize message using msgpack.packb()."""
        msg = [
            CHIRP_HEADER,
            self.msgtype.value,
            self.group_uuid.bytes,
            self.host_uuid.bytes,
            self.serviceid.value,
            self.port,
        ]
        return msgpack.packb(msg)

    def unpack(self, msg):
        """Unpack and decode binary msg using msgpack.unpackb()."""
        _header, msgtype, group, host, service, port = msgpack.unpackb(msg)
        self.msgtype = CHIRPMessageType(msgtype)
        self.group_uuid = UUID(bytes=group)
        self.host_uuid = UUID(bytes=host)
        self.serviceid = CHIRPServiceIdentifier(service)
        self.port = port


class CHIRPBeaconTransmitter:
    """Class for broadcasting CHRIP messages.

    See docs/protocols/chirp.md for details.

    """

    def __init__(
        self,
        name: str,
        group: str,
    ) -> None:
        """Initialize attributes and open broadcast socket."""
        self._host_uuid = uuid5(NAMESPACE_DNS, name)
        self._group_uuid = uuid5(NAMESPACE_DNS, group)

        # whether or not to filter broadcasts on group
        self._filter_group = True

        # Create UPP broadcasting socket
        #
        # NOTE: Socket options are often OS-specific; the ones below were chosen
        # for supporting Linux-based systems.
        #
        self._sock = socket.socket(
            socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP
        )
        # on socket layer (SOL_SOCKET), enable re-using address in case
        # already bound (REUSEPORT)
        self._sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
        # enable broadcasting
        self._sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        # non-blocking (i.e. a timeout of 0.0 seconds for recv calls)
        self._sock.setblocking(0)
        # bind to all interfaces to listen to incoming broadcast.
        #
        # NOTE: this only works for IPv4
        #
        # TODO: consider to only bind the interface matching a given IP!
        # Otherwise, we risk announcing services that do not bind to the
        # interface they are announced on.
        self._sock.bind(("", CHIRP_PORT))

    @property
    def host(self) -> UUID:
        """Get the UUID of the host this transmitter was set up for."""
        return self._host_uuid

    @property
    def group(self) -> UUID:
        """Get the UUID of the Constellation group of this transmitter."""
        return self._group_uuid

    @property
    def filter(self) -> bool:
        """Whether or not incoming broadcasts are filtered on group."""
        return self._filter_group

    @filter.setter
    def filter(self, val: bool):
        """Whether or not incoming broadcasts are filtered on group."""
        self._filter_group = val

    def broadcast(
        self,
        serviceid: CHIRPServiceIdentifier,
        msgtype: CHIRPMessageType,
        port: int = 0,
    ) -> None:
        """Broadcast a given service."""
        msg = CHIRPMessage(msgtype, self._group_uuid, self._host_uuid, serviceid, port)
        self._sock.sendto(msg.pack(), ("<broadcast>", CHIRP_PORT))

    def listen(self) -> CHIRPMessage:
        """Listen in on CHIRP port and return message if data was received."""
        try:
            buf, from_address = self._sock.recvfrom(1024)
        except BlockingIOError:
            # no data waiting for us
            return None

        if from_address[1] != CHIRP_PORT:
            # NOTE: not sure this can happen with the way the socket is set up
            return None

        header = msgpack.unpackb(buf)[0]
        if not header == CHIRP_HEADER:
            raise RuntimeError(
                f"Received malformed CHIRP header by host {from_address}: {header}!"
            )

        # Unpack msg
        msg = CHIRPMessage()
        try:
            msg.unpack(buf)
        except Exception as e:
            raise RuntimeError(
                f"Received malformed message by host {from_address}: {e}"
            ) from e

        # ignore msg from this (our) host
        if self._host_uuid == msg.host_uuid:
            return None

        # optionally drop messages from other groups
        if self._filter_group and self._group_uuid != msg.group_uuid:
            return None

        msg.from_address = from_address[0]
        return msg

    def close(self):
        """Close the socket."""
        self._sock.close()