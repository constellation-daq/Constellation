# Constellation Monitoring Distribution Protocol

* Status: draft
* Editor: The Constellation authors

The Constellation Monitoring Distribution Protocol (CMDP) defines how hosts distribute and receive auxiliary information such as log messages and statistics data for monitoring purposes.

## Preamble

The key words “MUST”, “MUST NOT”, “REQUIRED”, “SHALL”, “SHALL NOT”, “SHOULD”, “SHOULD NOT”, “RECOMMENDED”, “MAY”, and “OPTIONAL” in this document are to be interpreted as described in [RFC 2119](http://tools.ietf.org/html/rfc2119).

## Goals

This specification is intended to formally document the names and expected behaviour of the monitoring message distribution between hosts of the Constellation framework.
This protocol specifies how CMDP hosts are

* sending and receiving multicast messages with auxiliary status information to and from other CMDP hosts,
* formatting message topics to allow identification of the message type as well as filtering, and
* subscribing to specific message topics to reduce the number of transmitted messages and consequently the network load.

This protocol defines two message types which differ in purpose and message syntax:

* Log messages are intended for the distribution of informational logging messages from a CMDP host
* Metrics data messages are intended for the distribution of machine-readable monitoring data from a CMDP host.

Conforming implementations of this protocol SHOULD respect this specification, thus ensuring that applications can depend on predictable behavior.
This specification is not transport specific, but not all behaviour will be reproducible on all transports.

### Related Specifications

* [23/ZMTP](http://rfc.zeromq.org/spec:23/ZMTP) defines the message transport protocol.
* [29/PUBSUB](http://rfc.zeromq.org/spec:29/PUBSUB) defines the semantics of PUB, XPUB, SUB and XSUB sockets.
* [CHIRP](https://gitlab.desy.de/constellation/constellation/-/blob/main/docs/protocols/chirp.md) defines the network discovery protocol and procedure.

## Implementation

A CMDP message MUST consist of three frames and SHALL be sent as multipart message.
The definitions of ‘frame’ and multipart message’ follow those defined in [23/ZMTP](http://rfc.zeromq.org/spec:23/ZMTP).

The message SHALL consist of the following frames, in this order:

* The message topic
* The header header
* The message payload

### Overall Behavior

* Send only messages with topic that has subscribers, suppress others
* discard messages with invalid topic
* discard messages in congestion

The first frame of the message SHALL consist of the message topic in order to allow outgoing message filtering as specified in [29/PUBSUB](http://rfc.zeromq.org/spec:29/PUBSUB).
The sending CMDP host SHALL only send a message if the receiving CMDP host has a subscription to the topic of the message.
The topic MUST start either with prefix `LOG` or with prefix `STAT`.
A receiving CMDP host SHALL discard messages that it receives with a topic prefix different from these.

### Log Message Topic

Messages which start with topic prefix `LOG`, hereafter referred to as ‘log messages’, SHALL be used for the distribution of informational messages from the program flow of the sending CMDP host.
The `LOG` topic prefix MUST be followed by a trailing slash `/`, which SHALL be followed by a log level.
The log level SHOULD be any of `ERROR`, `STATUS`, `WARNING`, `INFO`, `DEBUG` or `TRACE`.

The following lists provides guidance on the usage of these topics:

* The topic `LOG/TRACE` SHOULD be used for verbose information which allows to follow the call stack of the host program.
* The topic `LOG/DEBUG` SHOULD be used for information relevant to developers for debugging the host program.
* The topic `LOG/INFO` SHOULD contain information on regular events intended for end users of the host program.
* The topic `LOG/WARNING` SHOULD be used to notify the end user of the host program of unexpected events which require further investigation.
* The topic `LOG/STATUS` SHOULD be used to communicate important information about the host program to the end user with low frequency.
* The topic `LOG/ERROR` SHOULD be used to notify the end user about critical events which require immediate attention and MAY have triggered an automated response by the host program or other hosts.

The log message topic MAY be followed by a trailing slash `/` and a component identifier of the host program to allow further filtering of messages.
An example for a valid log message topic with component identifier is `LOG/TRACE/NETWORKING`.

### Metrics Data Topic

Messages which start with topic prefix `STAT` SHALL be used for the distribution of metrics data messages from the sending host for the purpose of monitoring.
The `STAT` topic prefix MUST be followed by a trailing slash `/`, which SHALL be followed by a metrics name which SHOULD be unique per sending host.
It is RECOMMENDED to use the same unique metrics name across different hosts for the same metrics data quantity.

An example for a valid metrics data message topic is `STAT/CPULOAD`.

### Header

### Payload (Third Frame)


### Subscription to Monitoring Messages

