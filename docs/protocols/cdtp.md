# Constellation Data Transmission Protocol

* Status: draft
* Editor: The Constellation authors

The Constellation Data Transmission Protocol (CDTP) defines how data is transmitted from one sending host to one receiving host.

## Preamble

The key words “MUST”, “MUST NOT”, “REQUIRED”, “SHALL”, “SHALL NOT”, “SHOULD”, “SHOULD NOT”, “RECOMMENDED”, “MAY”, and “OPTIONAL” in this document are to be interpreted as described in [RFC 2119](http://tools.ietf.org/html/rfc2119).

## Goals

This specification is intended to formally document the names and expected behavior of the data message transmission between two hosts of the Constellation framework.
This protocol specifies how CDTP hosts are sending and receiving messages with cargo payload to and from other CDTP hosts.

This protocol defines three data message types which differ in purpose and message syntax:

* Begin-of-run messages indicate the start of a measurement undertaken by the sending CDTP host.
* Data messages contain sequential data recorded by the sending CDTP host.
* End-of-run messages mark the end of a measurement by the sending CDTP host.

Conforming implementations of this protocol SHOULD respect this specification, thus ensuring that applications can depend on predictable behavior.
This specification is not transport specific, but not all behavior will be reproducible on all transports.

### Related Specifications

* [23/ZMTP](http://rfc.zeromq.org/spec:23/ZMTP) defines the message transport protocol.
* [30/PIPELINE](http://rfc.zeromq.org/spec:30/PIPELINE) defines the semantics of PUSH and PULL sockets.
* [CHIRP](https://gitlab.desy.de/constellation/constellation/-/blob/main/docs/protocols/chirp.md) defines the network discovery protocol and procedure.
* [MessagePack](https://github.com/msgpack/msgpack/blob/master/spec.md) defines the encoding for data structures.

## Implementation

### Overall Behavior

A CDTP sender host SHALL advertise its CDTP service through [CHIRP](https://gitlab.desy.de/constellation/constellation/-/blob/main/docs/protocols/chirp.md) with service identifier `%x04`.

Upon service discovery through [CHIRP](https://gitlab.desy.de/constellation/constellation/-/blob/main/docs/protocols/chirp.md), a CDTP receiver host SHOULD connect its PULL socket to the PUSH socket of one CDTP sender host as defined by [30/PIPELINE](http://rfc.zeromq.org/spec:30/PIPELINE).

In case of network congestion, unsent messaged SHALL be buffered by the sending CDTP host and sent at a later time.
Upon reaching the high-water mark of buffered messages, the user MUST be notified and further sending of messages SHALL be blocked until action has been taken.

Any data message must be enclosed by the begin-of-run message and end-of-run message of the current measurement.
If a CDTP host receives a data message before the begin-of-run message, or if it receives a data message after the end-of-run message, the user MUST be notified and further reception of messages SHALL be blocked until action has been taken.

### Message Content

Any CDTP message SHALL be sent as a single ZeroMQ message. The message MUST be encoded according to the [MessagePack](https://github.com/msgpack/msgpack/blob/master/spec.md) specification. It SHALL contain two strings, followed by a one-byte message type identifier encoded as integer, and an array.

The first string MUST contain the protocol identifier, which SHALL consist of the letters ‘C’, ‘D’, ‘T’ and ‘P’, followed by the protocol version number, which SHALL be `%x02`.

The second string SHOULD contain the name of the sending CDTP host.

The message type identifier SHALL be either `%x00` (dubbed ‘DATA‘ for data), `%x01` (dubbed ‘BOR’ for begin-of-run), or `%x02` (dubbed ‘EOR’ for end-of-run).

The array shall contain any number of ‘Data Records’, which will be described below.

### Data Records

A data record represent a measurement point. It is an object encoded according to the [MessagePack](https://github.com/msgpack/msgpack/blob/master/spec.md) specification. It consists of an array with a fixed length of 3, containing an integer, a map and an array.

The integer SHALL contain the data record sequence number. It SHOULD be monotonically incremented number that represents the number of data records in DATA messages sent since the beginning of the measurement, starting with 1.

The map MAY contain a sequence of key-value pairs.
The key MUST be of string-type and the values MAY be any of the types supported by the [MessagePack](https://github.com/msgpack/msgpack/blob/master/spec.md) specification.

The array MAY contain any number of byte arrays, representing the measurement data.

### BOR and EOR Messages

BOR and EOR type messages SHALL contain exactly two data records which SHALL NOT contain any measurement data.

For BOR type messages, the map of the second data record SHALL contain the configuration of the CDTP sender host.

For EOR type messages, the map of the second data record MAY contain additional meta information of the measurement run.
