# Constellation Heartbeat Protocol

* Status: draft
* Editor: The Constellation authors

The Constellation Heartbeat Protocol (CHP) defines how hosts distribute and receive status information and liveliness indications for the purpose of tracking availability and uptime.

## Preamble

The key words “MUST”, “MUST NOT”, “REQUIRED”, “SHALL”, “SHALL NOT”, “SHOULD”, “SHOULD NOT”, “RECOMMENDED”, “MAY”, and “OPTIONAL” in this document are to be interpreted as described in [RFC 2119](http://tools.ietf.org/html/rfc2119).

## Goals

This specification is intended to formally document the names and expected behavior of the distribution and reception of heartbeat signals between hosts of the Constellation framework.

This protocol specifies how CHP hosts are sending and receiving multicast messages with host state information to and from other CHP hosts, and how these messages are formatted.

Conforming implementations of this protocol SHOULD respect this specification, thus ensuring that applications can depend on predictable behavior.
This specification is not transport specific, but not all behavior will be reproducible on all transports.

### Related Specifications

* [23/ZMTP](http://rfc.zeromq.org/spec:23/ZMTP) defines the message transport protocol.
* [29/PUBSUB](http://rfc.zeromq.org/spec:29/PUBSUB) defines the semantics of PUB, XPUB, SUB and XSUB sockets.
* [CHIRP](https://gitlab.desy.de/constellation/constellation/-/blob/main/docs/protocols/chirp.md) defines the network discovery protocol and procedure.

## Implementation

A CHP message SHALL be sent as multipart message and MUST consist of at least one frame and MAY consist of one additional frame.
The definitions of ‘frame’ and ‘multipart message’ follow those defined in [23/ZMTP](http://rfc.zeromq.org/spec:23/ZMTP).

### Overall Behavior

A CHP sending host SHALL advertise its CHP service through [CHIRP](https://gitlab.desy.de/constellation/constellation/-/blob/main/docs/protocols/chirp.md) with service identifier `%x02`.

Upon service discovery through [CHIRP](https://gitlab.desy.de/constellation/constellation/-/blob/main/docs/protocols/chirp.md), a CHP receiving host MAY subscribe to the CHP sending hosts as defined by [29/PUBSUB](http://rfc.zeromq.org/spec:29/PUBSUB).

A CHP sending host MUST publish messages to all subscribed CHP receiving hosts in regular time intervals. These messages are called "heartbeats". The CHP sending host SHALL publish messages over the entirety of its existence.

A CHP sending host SHOULD publish additional messages whenever its internal state changes. These messages are called "extrasystoles".

A receiving CHP host SHALL discard messages that it receives with an invalid formatting or content.

### Message Content

The heartbeat and extrasystole message frame MUST be encoded according to the MessagePack specification.
It SHALL contain two strings, followed by a 64-bit timestamp, two 1-OCTET integer values and a 2-OCTET integer value.

The first string MUST contain the protocol identifier, which SHALL consist of the letters ‘C’, ‘H’ and ‘P’, followed by the protocol version number, which SHALL be `%x01`.

The second string SHOULD contain the name of the sending CHP host.

The timestamp SHALL follow the MessagePack specification for timestamps and contain a 64-bit UNIX epoch timestamp in units of nanoseconds.

The values SHOULD be the time of sending the heartbeat or extrasystole message at the sending CHP host.

The first 1-OCTET integer variable SHALL contain the current state of the CHP sending host.

The second 1-OCTET integer variable SHALL hold the CHP message flags, which are defined as follows:

* `%x01` for message flag `DENY_DEPARTURE`: When this sender departs, an interrupt should be triggered.
* `%x02` for message flag `TRIGGER_INTERRUPT`: Any issue encountered with this sender should trigger an interrupt.
* `%x04` for message flag `MARK_DEGRADED`: Any issue encountered with this sender should mark data as degraded.
* `%x80` for message flag `IS_EXTRASYSTOLE`: The currently processed CHP message is an extrasystole message.
* The flags `%x08`, `%x10`, `%x20` and `%x40` are reserved for future extensions to the protocol.

The 2-OCTET integer variable SHALL indicate the maximum time interval in units of milliseconds until the next heartbeat message is emitted by the sending CHP host.

### Payload Frame

The OPTIONAL message payload frame SHALL consist of the current status message of the CHP sender, encoded as a UTF8 string.

### Lives Counter & Heartbeat Timeouts

A receiving CHP host SHALL keep a counter and the current state of each CHP sending host it is subscribed to. The counter value is referred to as "lives". Upon subscription the lives counter for the corresponding CHP sending host SHALL be set to an initial value.

Whenever no heartbeat or extrasystole message is received within the defined time interval, the CHP receiving host SHALL reduce the lives counter by one. Upon reaching a lives counter value of zero, the CHP sending host SHALL be considered unavailable.

Whenever a heartbeat or extrasystole message is received within the defined time interval, the lives counter SHALL be reset to its initial value and the current state SHALL be updated with the state information of the CHP sending host from the message.

The time intervals for publishing heartbeat messages by CHP sending hosts, called "heart rate", SHALL be variable and adjustable over time by the CHP sending host. Heartbeat messages MAY be sent earlier than the indicated time interval and additional extrasystole messages MAY be published anytime.

The maximum time interval expected for receiving the next heartbeat messages from a given CHP sending host SHALL be taken from the heart rate value of the last received heartbeat or extrasystole message and SHALL be adjusted with every received message.

The initial value of the lives counter SHOULD be 3.

### Congestion Control

A sending CHP host MAY adjust the time interval automatically as a function of the number of active subscribers in order to reduce the total number of heartbeat messages transmitted per unit time. In the absence of such automatic adjustment, the number of messages in the network scales as {math}`N(N-1)` where {math}`N` is the number of participating CHP hosts.

To mitigate this scaling behavior, it is RECOMMENDED that the heartbeat interval be scaled approximately with the square root of the number of CHP hosts. A suitable formula for this adjustment is:

```{math}
\Delta t = \min \left(\Delta t_{max}, \max \left( \Delta t_{min}, \Delta t_{min} \cdot \sqrt{N - 1} \cdot l \right) \right)
```

where {math}`\Delta t` is the computed heartbeat time interval to be used by the sender, {math}`\Delta t_{min}` and {math}`\Delta t_{max}` the minimum and maximum allowed heartbeat time intervals, respectively, and {math}`l` is a load factor which MAY be used to tune the weight of the number of CHP hosts.

It is RECOMMENDED to use a maximum heartbeat time interval {math}`\Delta t_{max}` of 30s. This value MAY be adapted according the requirements of the deployment.

If congestion control is implemented, the implementation MUST ensure to first distribute a heartbeat message with the updated interval before switching to the
interval to avoid detecting missing heartbeats on CHP receiving hosts.
