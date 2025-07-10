"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2

Module implementing the Constellation Host Identification and Reconnaissance Protocol (CHIRP).
"""

import io
from enum import Enum
from hashlib import md5
from uuid import UUID

from .multicast import MulticastSocket

CHIRP_PORT = 7123
CHIRP_MULTICAST_ADDRESS = "239.192.7.123"
CHIRP_HEADER = "CHIRP\x01"


def get_uuid(name: str) -> UUID:
    """Return the UUID for a string using MD5 hashing."""
    hash = md5(name.encode(), usedforsecurity=False)
    return UUID(bytes=hash.digest())


class CHIRPServiceIdentifier(Enum):
    """Identifies the type of service.

    The CONTROL service identifier indicates a CSCP (Constellation Satellite
    Control Protocol) service.

    The HEARTBEAT service identifier indicates a CHP (Constellation Heartbeat
    Protocol) service.

    The MONITORING service identifier indicates a CMDP (Constellation Monitoring
    Distribution Protocol) service.

    The DATA service identifier indicates a CDTP (Constellation Data
    Transmission Protocol) service.

    The NONE identifier is used for initialization only, and is not a valid
    service type.

    """

    NONE = 0x0
    CONTROL = 0x1
    HEARTBEAT = 0x2
    MONITORING = 0x3
    DATA = 0x4


class CHIRPMessageType(Enum):
    """Identifies the type of message sent or received via the CHIRP protocol.

    See `docs/protocols/chirp.md` for details.

    REQUEST: A message with REQUEST type indicates that CHIRP hosts should reply
    with an OFFER

    OFFER: A message with OFFER type indicates that service is available

    DEPART: A message with DEPART type indicates that a service is no longer
    available

    NONE: Value used for initialization only, not a valid message type.

    """

    NONE = 0x0
    REQUEST = 0x1
    OFFER = 0x2
    DEPART = 0x3


class CHIRPMessage:
    """Class to hold a CHIRP message."""

    def __init__(
        self,
        msgtype: CHIRPMessageType = CHIRPMessageType.NONE,
        group_uuid: UUID = UUID(int=0),
        host_uuid: UUID = UUID(int=0),
        serviceid: CHIRPServiceIdentifier = CHIRPServiceIdentifier.NONE,
        port: int = 0,
    ):
        """Initialize attributes."""
        self.msgtype = msgtype
        self.group_uuid = group_uuid
        self.host_uuid = host_uuid
        self.serviceid = serviceid
        self.port = port
        self.from_address: str = ""

    def pack(self) -> bytes:
        """Serialize message to raw bytes."""
        bytes = io.BytesIO()
        bytes.write(CHIRP_HEADER.encode())
        bytes.write(self.msgtype.value.to_bytes(length=1))
        bytes.write(self.group_uuid.bytes)
        bytes.write(self.host_uuid.bytes)
        bytes.write(self.serviceid.value.to_bytes(length=1))
        bytes.write(self.port.to_bytes(length=2, byteorder="big"))
        return bytes.getvalue()

    def unpack(self, msg: bytes) -> None:
        """Decode from bytes."""
        # Check message length
        if len(msg) != 42:
            raise RuntimeError(f"Invalid CHIRP message: length is {len(msg)} instead of 42 bytes long")
        # Check header
        if msg[0:6] != CHIRP_HEADER.encode():
            raise RuntimeError(f"Invalid CHIRP message: header {msg[0:6]!r} is malformed")
        # Decode message
        self.msgtype = CHIRPMessageType(int.from_bytes(msg[6:7]))
        self.group_uuid = UUID(bytes=msg[7:23])
        self.host_uuid = UUID(bytes=msg[23:39])
        self.serviceid = CHIRPServiceIdentifier(int.from_bytes(msg[39:40]))
        self.port = int.from_bytes(msg[40:42], byteorder="big")

    def __str__(self) -> str:
        """Pretty-print CHIRP message."""
        s = "CHIRP message from {} received of type {}, "
        s += "host id {}, service id {} on port {}."
        return s.format(
            self.from_address,
            self.msgtype,
            self.host_uuid,
            self.serviceid,
            self.port,
        )


class CHIRPBeaconTransmitter:
    """Class for sending CHIRP multicast messages.

    See `docs/protocols/chirp.md` for details.

    """

    def __init__(
        self,
        name: str,
        group: str,
        interface_addresses: list[str],
    ) -> None:
        """Initialize attributes and open multicast socket."""
        self._host_uuid = get_uuid(name)
        self._group_uuid = get_uuid(group)

        # whether or not to filter multicasts on group
        self._filter_group = True

        # Create multicast socket
        self._socket = MulticastSocket(interface_addresses, CHIRP_MULTICAST_ADDRESS, CHIRP_PORT)

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
        """Whether or not incoming messages are filtered on group."""
        return self._filter_group

    @filter.setter
    def filter(self, val: bool) -> None:
        """Whether or not incoming messages are filtered on group."""
        self._filter_group = val

    def emit(
        self,
        serviceid: CHIRPServiceIdentifier,
        msgtype: CHIRPMessageType,
        port: int = 0,
    ) -> None:
        """Emit a message for the given service."""
        msg = CHIRPMessage(msgtype, self._group_uuid, self._host_uuid, serviceid, port)
        self._socket.sendMessage(msg.pack())

    def listen(self) -> CHIRPMessage | None:
        """Listen in on CHIRP port and return message if data was received."""
        multicast_message = self._socket.recvMessage()
        if multicast_message is None:
            return None

        # Unpack msg
        msg = CHIRPMessage()
        try:
            msg.unpack(multicast_message.content)
        except Exception as e:
            raise RuntimeError(f"Received malformed message by host {multicast_message.address}: {e}") from e

        # ignore msg from this (our) host
        if self._host_uuid == msg.host_uuid:
            return None

        # optionally drop messages from other groups
        if self._filter_group and self._group_uuid != msg.group_uuid:
            return None

        msg.from_address = multicast_message.address
        return msg

    def close(self) -> None:
        """Close the socket."""
        self._socket.close()
