# Constellation Data Transmission Protocol

* Status: draft
* Editor: The Constellation authors

The Constellation Data Transmission Protocol (CDTP) defines how data is transmitted from one sending host to one receiving host.

## Preamble

The key words “MUST”, “MUST NOT”, “REQUIRED”, “SHALL”, “SHALL NOT”, “SHOULD”, “SHOULD NOT”, “RECOMMENDED”, “MAY”, and “OPTIONAL” in this document are to be interpreted as described in [RFC 2119](http://tools.ietf.org/html/rfc2119).

## Goals

This specification is intended to formally document the names and expected behaviour of the data message transmission between two hosts of the Constellation framework.
This protocol specifies how CDTP hosts are sending and receiving messages with cargo payload to and from other CDTP hosts.

This protocol defines the data message type and its syntax.

Conforming implementations of this protocol SHOULD respect this specification, thus ensuring that applications can depend on predictable behavior.
This specification is not transport specific, but not all behaviour will be reproducible on all transports.

### Related Specifications

* [23/ZMTP](http://rfc.zeromq.org/spec:23/ZMTP) defines the message transport protocol.
* [30/PIPELINE](http://rfc.zeromq.org/spec:30/PIPELINE) defines the semantics of PUSH and PULL sockets.
* [CHIRP](https://gitlab.desy.de/constellation/constellation/-/blob/main/docs/protocols/chirp.md) defines the network discovery protocol and procedure.
* [MessagePack](https://github.com/msgpack/msgpack/blob/master/spec.md) defines the encoding for data structures.

## Implementation

A CDTP message SHALL be sent as multipart message and MUST consist of at least two frames and MAY consist of any larger number of frames.
The definitions of ‘frame’ and ‘multipart message’ follow those defined in [23/ZMTP](http://rfc.zeromq.org/spec:23/ZMTP).

The message SHALL consist at least of the following frames, in this order:

* The message header
* The message cargo payload

OPTIONAL frames with additional cargo payload MAY be sent afterwards.

### Overall Behavior

A CDTP service provider host SHALL advertise its CDTP service through [CHIRP](https://gitlab.desy.de/constellation/constellation/-/blob/main/docs/protocols/chirp.md).

Upon service discovery through [CHIRP](https://gitlab.desy.de/constellation/constellation/-/blob/main/docs/protocols/chirp.md), a CDTP receiver host SHOULD connect its PULL socket to the PUSH socket of one CDTP sender host as defined by [30/PIPELINE](http://rfc.zeromq.org/spec:30/PIPELINE).

A CDTP receiver host SHALL notify the user about messages that it receives with an invalid header.

In case of network congestion, unsent messaged SHALL be buffered by the sending CDTP host and sent at a later time.

### Message Header

The message header frame MUST be encoded according to the [MessagePack](https://github.com/msgpack/msgpack/blob/master/spec.md) specification.
It SHALL contain two strings, followed by a 64-bit timestamp and a map.

The first string MUST contain the protocol identifier, which SHALL consist of the letters ‘C’, ‘D’, ‘T’ and ‘P’, followed by the protocol version number, which SHALL be %x01.

The second string SHOULD contain the name of the sending CDTP host.

The timestamp SHALL follow the [MessagePack](https://github.com/msgpack/msgpack/blob/master/spec.md) specification for timestamps and contain a 64-bit UNIX epoch timestamp in units of nanoseconds.
Possible values MAY be the time of sending the message or the time of generation of the payload at the sending CMDP host.

The map MAY contain a sequence of key-value pairs.
The key MUST be of string-type and the values MAY be any of the types supported by the [MessagePack](https://github.com/msgpack/msgpack/blob/master/spec.md) specification.

### Message Payload

The message cargo payload frames MAY consist of any binary data.
The interpretation and decoding of this data is not part of this protocol and left for user code implementations.

