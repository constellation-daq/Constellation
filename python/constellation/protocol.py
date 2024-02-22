#!/usr/bin/env python3
"""Module implementing the Constellation communication protocols."""

import msgpack
import zmq
import socket
import time
import platform
import logging
from uuid import UUID
from enum import Enum


PROTOCOL_IDENTIFIER = "CDTP%x01"  # TODO: Change PROTOCOL_IDENTIFIER to include all other protocols and change all methods in file to check for correct protocol
CHIRP_PORT = 7123
CHIRP_HEADER = "CHIRP%x01"


class MessageHeader:
    """Class implementing a Constellation message header."""

    def __init__(self, host: str = None):
        if not host:
            host = platform.node()
        self.host = host

    def send(self, socket: zmq.Socket, flags: int = zmq.SNDMORE, meta: dict = None):
        """Send a message header via socket.

        Returns: return value from socket.send()."""
        return socket.send(self.encode(meta), flags)

    def recv(self, socket: zmq.Socket, flags: int = 0):
        """Receive header from socket and return all decoded fields."""
        return self.decode(self.queue.recv())

    def decode(self, header):
        """Decode header string and return host, timestamp and meta map."""
        if not header[0] == PROTOCOL_IDENTIFIER:
            raise RuntimeError(
                f"Received message with malformed CDTP header: {header}!"
            )
        header = msgpack.unpackb(header)
        host = header[1]
        timestamp = header[2]
        meta = header[3]
        return host, timestamp, meta

    def encode(self, meta: dict = None):
        """Generate and return a header as list."""
        if not meta:
            meta = {}
        header = [
            PROTOCOL_IDENTIFIER,
            self.host,
            msgpack.Timestamp.from_unix_nano(time.time_ns()),
            meta,
        ]
        return msgpack.packb(header)


class DataTransmitter:
    """Base class for sending Constellation data packets via ZMQ."""

    def __init__(self, socket: zmq.Socket = None, host: str = None):
        """Initialize transmitter.

        socket: the ZMQ socket to use if no other is specified on send()/recv()
        calls.

        host: the name to use in the message header. Defaults to system host name.

        """
        if not host:
            host = platform.node()
        self.host = host
        self._socket = socket

    def send(
        self, payload, meta: dict = None, socket: zmq.Socket = None, flags: int = 0
    ):
        """Send a payload over a ZMQ socket.

        Follows the Constellation Data Transmission Protocol.

        payload: data to send.

        meta: dictionary to include in the map of the message header.

        socket: ZMQ socket to use for transmission. If none is specified, use
        the one the class was initialized with.

        flags: additional ZMQ socket flags to use during transmission.

        Returns: return of socket.send(payload) call.

        """
        if not meta:
            meta = {}
        # use default socket if none was specified
        if not socket:
            socket = self._socket
        flags = zmq.SNDMORE | flags
        # message header
        socket.send(msgpack.packb(PROTOCOL_IDENTIFIER), flags=flags)
        socket.send(msgpack.packb(self.host), flags=flags)
        socket.send(msgpack.packb(time.time_ns()), flags=flags)
        socket.send(msgpack.packb(meta), flags=flags)
        # payload
        flags = flags & (~zmq.SNDMORE)  # flip SNDMORE bit
        return socket.send(msgpack.packb(payload), flags=flags)

    def recv(self, socket: zmq.Socket = None, flags: int = 0):
        """Receive a multi-part data transmission.

        Follows the Constellation Data Transmission Protocol.

        socket: ZMQ socket to use for transmission. If none is specified, use
        the one the class was initialized with.

        flags: additional ZMQ socket flags to use during transmission.

        Returns: payload, map (meta data), timestamp and sending host.

        """
        # use default socket if none was specified
        if not socket:
            socket = self._socket
        msg = socket.recv_multipart(flags=flags)
        if not len(msg) == 5:
            raise RuntimeError(
                f"Received message with wrong length of {len(msg)} parts!"
            )
        if not msgpack.unpackb(msg[0]) == PROTOCOL_IDENTIFIER:
            raise RuntimeError(
                f"Received message with malformed CDTP header: {msgpack.unpackb(msg[0])}!"
            )
        payload = msgpack.unpackb(msg[4])
        meta = msgpack.unpackb(msg[3])
        ts = msgpack.unpackb(msg[2])
        host = msgpack.unpackb(msg[1])
        return payload, meta, host, ts

    def close(self):
        """Close the socket."""
        self._socket.close()


class LogTransmitter:
    """Class for sending Constellation log messages via ZMQ."""

    def __init__(self, socket: zmq.Socket = None, host: str = None):
        """Initialize transmitter.

        socket: the ZMQ socket to use if no other is specified on send()/recv()
        calls.

        host: the name to use in the message header. Defaults to system host name.

        """
        if not host:
            host = platform.node()
        self.host = host
        self._socket = socket

    def send(self, record: logging.LogRecord, socket: zmq.Socket = None):
        """Send a LogRecord via an ZMQ socket.

        Follows the Constellation Monitoring Distribution Protocol.

        record: LogRecord to send.

        socket: ZMQ socket to use for transmission. If none is specified, use
        the one the class was initialized with.

        Returns: return of socket.send(record) call.

        """
        # use default socket if none was specified
        if not socket:
            socket = self._socket
        topic = f"LOG/{record.levelname}/{record.name}"
        header = [
            PROTOCOL_IDENTIFIER,
            self.host,
            msgpack.Timestamp.from_unix_nano(time.time_ns()),
            {},
        ]
        socket.send_string(topic, zmq.SNDMORE)
        socket.send(msgpack.packb(header), zmq.SNDMORE)
        # Instead of just adding the formatted message, this adds key attributes
        # of the LogRecord, allowing to reconstruct the full message on the
        # other end.
        # TODO filter and name these according to the Constellation LOG specifications
        return socket.send(
            msgpack.packb(
                {
                    "name": record.name,
                    "msg": record.msg,
                    "args": record.args,
                    "levelname": record.levelname,
                    "levelno": record.levelno,
                    "pathname": record.pathname,
                    "filename": record.filename,
                    "module": record.module,
                    "exc_info": record.exc_info,
                    "exc_text": record.exc_text,
                    "stack_info": record.stack_info,
                    "lineno": record.lineno,
                    "funcName": record.funcName,
                    "created": record.created,
                    "msecs": record.msecs,
                    "relativeCreated": record.relativeCreated,
                    "thread": record.thread,
                    "threadName": record.threadName,
                    "processName": record.processName,
                    "process": record.process,
                    "message": record.message,
                }
            )
        )

    def recv(self, socket: zmq.Socket = None):
        """Receive a Constellation log message and return a LogRecord.

        Follows the Constellation Monitoring Distribution Protocol.

        socket: ZMQ socket to use for transmission. If none is specified, use
        the one the class was initialized with.

        Returns: LogRecord.

        """
        # use default socket if none was specified
        if not socket:
            socket = self._socket
        topic = msgpack.unpackb(self.queue.recv())
        # NOTE it is currently not possible to receive both STATS and LOG on the
        # same socket as the respective classes do not handle the other.
        # TODO : refactorize LogTransmitter to allow to transparently receive logs and stats
        if not topic.startswith("LOG/"):
            raise RuntimeError(
                f"LogTransmitter cannot decode messages of topic '{topic}'"
            )
        header = msgpack.unpackb(self.queue.recv())
        if not header[0] == PROTOCOL_IDENTIFIER:
            raise RuntimeError(
                f"Received message with malformed CDTP header: {header}!"
            )
        record = msgpack.unpackb(self.queue.recv())
        return logging.makeLogRecord(record)

    def closed(self) -> bool:
        """Return whether socket is closed or not."""
        if self._socket:
            return self._socket.closed
        return True

    def close(self):
        """Close the socket."""
        self._socket.close()


class MetricsType(Enum):
    LAST_VALUE = 0x1
    ACCUMULATE = 0x2
    AVERAGE = 0x3
    RATE = 0x4


class Metric:
    """Class to hold information for a Constellation metric."""

    def __init__(
        self, name: str, description: str, unit: str, handling: MetricsType, value=None
    ):
        self.name = name
        self.description = description
        self.unit = unit
        self.handling = handling
        self.value = value


class MetricsTransmitter:
    """Class to send and receive a Constellation Metric."""

    def __init__(self, socket: zmq.Socket = None, host: str = None) -> None:
        """Initialize transmitter.

        socket: the ZMQ socket to use if no other is specified on send()/recv()
        calls.

        host: the name to use in the message header. Defaults to system host name.

        """
        if not host:
            host = platform.node()
        self.host = host
        self._socket = socket

    def send(self, metric: Metric, socket: zmq.Socket = None):
        """Send a metric via a ZMQ socket.

        Follows the Constellation Monitoring Distribution Protocol.

        metric: Metric to send.

        socket: ZMQ socket to use for transmission. If none is specified, use
        the one the class was initialized with.

        Returns: return of socket.send(metric) call.

        """
        # use default socket if none was specified
        if not socket:
            socket = self._socket
        topic = "STATS/" + metric.name
        header = [
            PROTOCOL_IDENTIFIER,
            self.host,
            msgpack.Timestamp.from_unix_nano(time.time_ns()),
            {},
        ]
        self._socket.send_string(topic, zmq.SNDMORE)
        self.queue.send(msgpack.packb(header), zmq.SNDMORE)
        self.queue.send(
            msgpack.packb(
                [metric.description, metric.unit, metric.handling.value, metric.value]
            )
        )

    def recv(self, socket: zmq.Socket = None):
        """Receive a Constellation STATS message and return a Metric.

        Follows the Constellation Monitoring Distribution Protocol.

        socket: ZMQ socket to use for transmission. If none is specified, use
        the one the class was initialized with.

        Returns: Metric.

        """
        # use default socket if none was specified
        if not socket:
            socket = self._socket
        topic = msgpack.unpackb(self.queue.recv())
        # NOTE it is currently not possible to receive both STATS and LOG on the
        # same socket as the respective classes do not handle the other.
        # TODO : refactorize MetricsTransmitter to allow to transparently receive logs and stats
        if not topic.startswith("STATS/"):
            raise RuntimeError(
                f"MetricsTransmitter cannot decode messages of topic '{topic}'"
            )
        name = topic.split("/")[1]
        header = msgpack.unpackb(self.queue.recv())
        if not header[0] == PROTOCOL_IDENTIFIER:
            raise RuntimeError(
                f"Received message with malformed CDTP header: '{header}'!"
            )
        description, unit, handling, value = msgpack.unpackb(self.queue.recv())
        return Metric(name, description, unit, MetricsType(handling), value)

    def close(self):
        """Close the socket."""
        self._socket.close()


class CSCPMessageVerb(Enum):
    """Defines the message types of the CSCP.

    Part of the Constellation Satellite Control Protocol, see
    docs/protocols/cscp.md for details.

    """

    REQUEST = 0x0
    SUCCESS = 0x1
    NOTIMPLEMENTED = 0x2
    INCOMPLETE = 0x3
    INVALID = 0x4
    UNKNOWN = 0x5


class CommandTransmitter:
    """Class implementing Constellation Satellite Control Protocol."""

    def __init__(self, name: str, socket: zmq.Socket):
        self.msghead = MessageHeader(name)
        self.socket = socket

    def send_request(self, command, payload=None):
        """Send a command request to a Satellite with an optional payload."""
        self._dispatch(command, CSCPMessageVerb.REQUEST, flags=zmq.NOBLOCK)

    def send_reply(self, response, msgtype: CSCPMessageVerb, payload=None):
        """Send a reply to a previous command."""
        self._dispatch(response, msgtype, payload, flags=zmq.NOBLOCK)

    def get_request(self):
        cmdmsg = self.socket.recv_multipart(flags=zmq.NOBLOCK)
        host, timestamp, meta = self.msgheader.decode(cmdmsg[0])
        # TODO CONTINUE HERE!!

    def _dispatch(
        self, msg: str, msgtype: CSCPMessageVerb, payload=None, flags: int = 0
    ):
        flags = zmq.SNDMORE | flags
        self.msghead.send(self.socket, flags=flags)
        if not payload:
            flags = flags
        self.socket.send(
            msgpack.packb(f"%x{msgtype.value:02o}{msg}"),
            flags=flags,
        )
        if payload:
            self.socket.send(msgpack.packb(payload), flags=zmq.NOBLOCK)


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
        host_uuid: UUID,
        group_uuid: UUID,
    ) -> None:
        """Initialize attributes and open broadcast socket."""
        self._host_uuid = host_uuid
        self._group_uuid = group_uuid

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

    def broadcast(
        self,
        serviceid: CHIRPServiceIdentifier,
        msgtype: CHIRPMessageType,
        port: int = 0,
    ):
        """Broadcast a given service."""
        msg = CHIRPMessage(msgtype, self._group_uuid, self._host_uuid, serviceid, port)
        self._sock.sendto(msg.pack(), ("<broadcast>", CHIRP_PORT))

    def listen(self):
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
            )

        if self._host_uuid == msg.host_uuid:
            # ignore msg from this (our) host
            return None
        msg.from_address = from_address[0]
        # TODO decide and document what to return here
        return msg

    def close(self):
        """Close the socket."""
        self._sock.close()
