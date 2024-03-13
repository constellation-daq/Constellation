# Constellation Satellite Control Protocol

* Status: draft
* Editor: The Constellation authors

The Constellation Satellite Control Protocol (CSCP) defines how satellite hosts receive and respond to commands and command payload.

## Preamble

The key words “MUST”, “MUST NOT”, “REQUIRED”, “SHALL”, “SHALL NOT”, “SHOULD”, “SHOULD NOT”, “RECOMMENDED”, “MAY”, and “OPTIONAL” in this document are to be interpreted as described in [RFC 2119](http://tools.ietf.org/html/rfc2119).

## Goals

This specification is intended to formally document the names and expected behaviour of the communication pattern between a controller host and a satellite host of the Constellation framework.
This protocol specifies how

* the controller host sends requests with command verbs to the satellite host
* the satellite host responds to requests from controller hosts.

Conforming implementations of this protocol SHOULD respect this specification, thus ensuring that applications can depend on predictable behavior.
This specification is not transport specific, but not all behaviour will be reproducible on all transports.

### Related Specifications

* [23/ZMTP](http://rfc.zeromq.org/spec:23/ZMTP) defines the message transport protocol.
* [28/REQREP](http://rfc.zeromq.org/spec:28/REQREP) defines the semantics of the request-reply pattern and the REQ, REP sockets.
* [CHIRP](https://gitlab.desy.de/constellation/constellation/-/blob/main/docs/protocols/chirp.md) defines the network discovery protocol and procedure.
* [MessagePack](https://github.com/msgpack/msgpack/blob/master/spec.md) defines the encoding for data structures.

## Implementation

The implementation of this protocol for the controller host and the satellite host is defined as follows.

### Controller Host

The controller host SHALL implement a REQ socket as defined by [28/REQREP](http://rfc.zeromq.org/spec:28/REQREP) and SHALL act as the client for CSCP messages, sending requests and receiving replies to any number of CSCP satellite hosts.

General behavior:

* MAY be connected to any number of CSCP satellite hosts.
* SHALL send and then receive exactly one message at a time.

For processing outgoing messages:

* SHALL return a suitable error, when it has no connected satellites.
* SHALL NOT discard messages that it cannot send to a connected satellite.

For processing incoming messages:

* SHALL accept an incoming message only from the last satellite that it sent a request to.
* SHALL discard silently any messages received from other satellites.

### Satellite Host

The satellite host SHALL implement a REP socket as defined by [28/REQREP](http://rfc.zeromq.org/spec:28/REQREP) and SHALL act as service for a set of CSCP controller hosts, receiving requests and sending replies back to the requesting CSCP controller host.

A CSCP satellite host SHALL advertise its CSCP service through [CHIRP](https://gitlab.desy.de/constellation/constellation/-/blob/main/docs/protocols/chirp.md).

General behavior:

* MAY be connected to any number of CSCP controller hosts.
* SHALL receive and then send exactly one message at a time.

For processing incoming messages:

* SHALL receive incoming messages from its CSCP controller peers.

For processing outgoing messages:

* SHALL deliver a single reply message back to the originating CSCP controller host.
* SHALL silently discard the reply, or return an error, if the originating CSCP controller host is no longer connected.
* SHALL not block on sending.

### CSCP Message

A CSCP message SHALL be sent as multipart message and MUST consist of at least two frames and MAY consist of one additional frame.
The definitions of ‘frame’ and ‘multipart message’ follow those defined in [23/ZMTP](http://rfc.zeromq.org/spec:23/ZMTP).

The message SHALL consist at least of the following frames, in this order:

* The message header
* The message verb

One OPTIONAL frame with payload MAY be sent afterwards.

### Message Header

The message header frame MUST be encoded according to the [MessagePack](https://github.com/msgpack/msgpack/blob/master/spec.md) specification.
It SHALL contain two strings, followed by a 64-bit timestamp and a map.

The first string MUST contain the protocol identifier, which SHALL consist of the letters ‘C’, ‘S’, ‘C’ and ‘P’, followed by the protocol version number, which SHALL be `%x01`.

The second string SHOULD contain the name of the sending CSCP host.

The timestamp SHALL follow the [MessagePack](https://github.com/msgpack/msgpack/blob/master/spec.md) specification for timestamps and contain a 64-bit UNIX epoch timestamp in units of nanoseconds.
Possible values MAY be the time of sending the message or the time of generation of the payload at the sending CSCP host.

The map MAY contain a sequence of key-value pairs.
The key MUST be of string-type and the values MAY be any of the types supported by the [MessagePack](https://github.com/msgpack/msgpack/blob/master/spec.md) specification.

### Message Verb

The message verb SHALL contain an octet and a string, encoded according to the [MessagePack](https://github.com/msgpack/msgpack/blob/master/spec.md) specification.

The octet MUST contain the message type, which SHALL be `%x00` for requests and SHALL be any of the following acknowledgement codes for replies:

* `%x01` for acknowledgement verb `SUCCESS`: The command was successfully received and is executed.
* `%x02` for acknowledgement verb `NOTIMPLEMENTED`: The command is valid but not implemented by the replying satellite host.
* `%x03` for acknowledgement verb `INCOMPLETE`: The command is valid but mandatory payload information for this command is missing or incorrectly formatted.
* `%x04` for acknowledgement verb `INVALID`: The command is invalid for the current state of the replying satellite host, e.g. it does not represent a valid transition out of its current state
* `%x05` for acknowledgement verb `UNKNOWN`: The command is entirely unknown.
* `%x06` for acknowledgement verb `ERROR`: The received message is not valid.

For request messages, the string SHALL contain the command.
Commands SHALL be parsed and interpreted case-insensitive.

For reply messages, the string SHALL provide additional information on the acknowledgement.

### Message Payload

The interpretation and decoding of this data is not part of this protocol and left for user code implementations.
