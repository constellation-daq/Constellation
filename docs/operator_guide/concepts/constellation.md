# System Architecture

This section of the manual provides an overview of the concepts and system architecture of the Constellation framework.

Constellation is a network-distributed control and data acquisition framework geared towards flexibility and easy integration
of new sensors, detectors and other devices. The components of the framework run on a single computer or multiple machines
across a local IP-based network, and allow to distribute commands, record data and collect monitoring and logging information
from its different constituents.

## Communication Protocols

Constellation is built around a set of communication protocols defining the exchange of messages among its nodes. While the
technical implementation of these protocols is of no concern for operating a Constellation network, they are described here
in brevity to provide a better understanding of the workings. The technical details of the protocols are available in the
[Framework Development Guide](../../framework_reference/protocols) along with the protocol RFC documents in the
corresponding [Appendix](../../protocols/index).

### Network Discovery

A common nuisance in volatile networking environments with devices appearing and disappearing is the discovery of available
devices and services. Constellation uses its *CHIRP* protocol to find other nodes on the same network.
It is an IPv4 protocol intended to be used on local networks only which uses a set of defined beacon UDP messages to announce
or request services.
The protocol is defined such that at any time of the operation, new nodes can join and request information on a particular
service from the already running Constellation participants.
When services are shut down, the a special beacon will prompt other nodes to disconnect.

All other protocols use *CHIRP* in order to find the other nodes of the Constellation.

### Heartbeat Exchange

Autonomous operation of the Constellation requires the constant exchange of state information between all participants.
This is implemented in the form of heartbeat messages sent via the *CHP* protocol. Heartbeat messages contain the current
state of the sender finite state machine and the time interval after which the next heartbeat is to be expected.
With this information, heartbeat receivers can independently deduce the state of other nodes, or identify non-responsive
nodes in case of network or machine failure.

The receiving node expects a new heartbeat message from each of the senders within their announced heartbeat interval.
If it fails to receive such a message in time, an internal *lives counter* is decremented, and the corresponding sender is
considered non-responsive once zero lives are left. The successful reception of a heartbeat resets this counter to its
initial value.

In addition to regular heartbeat patterns, so-called *extrasystoles* are sent whenever the state of the sender changes.
This enables an immediate reaction to remote state changes without having to wait for the next regular heartbeat update
interval.

A detailed description of the features implemented in Constellation based on the information transmitted by means of
heartbeat messages can be found in the [Autonomy Section](./autonomy.md).

### Control Commands

Commands are sent from controller instances to satellites via the *CSCP* protocol.
The command message consists of a message verb with a type and a command, and an optional payload to transmit data.
The controller exclusively sends messages with type `REQUEST`, while the satellite answers with a message type matching the
situation, such as `SUCCESS` when the command was executed successfully, `INVALID` if for example a transition was requested
that is not possible from the current state, or `UNKNOWN` in case the command is not known to the satellite.

The satellite command palette contains a set of standard commands to query properties and initiate state transitions, but can
be extended by the specific implementation as described in the [Satellite Section](./satellite.md].

### Data Transmission

Data are transferred within a Constellation network using the *CDTP* protocol.
The message format consists of a header frame with sender information and a payload that differs depending on the three
different message types:

* `BOR` - **Begin of Run**: This message is sent automatically at the start of a new measurement, i.e. upon entering the
  {bdg-secondary}`RUN` state of the FSM. It marks the start of a measurement in time and its payload frame contains the
  currently active satellite configuration for later reference, as well as other metadata such as the current run ID.
* `DATA`: This is the standard message type, consisting of the header frame and any number of so-called **data records**
  containing the data of the respective instrument. Each data record may contain multiple data blocks and is marked with an
  incrementing data sequence counter, providing the possibility for additional offline data integrity checks.
* `EOR` - **End of Run**: This message is sent automatically at the end of a measurement, i.e. when the sending satellite\
  leaves the {bdg-secondary}`RUN`. It contains metadata collected by the satellite over the course of the run.

### Logging & Telemetry

The distribution of log messages and telemetry data within Constellation is handled by the *CMDP* protocol.
The protocol supports topics, which allows to select only the relevant slice of information from an otherwise verbose
communication, and therefore reduces the required network bandwidth.

Listeners subscribe to logging levels or telemetry topics and consequently only these messages are transmitted over the network.
These subscriptions are completely independent of other protocols and can be performed at any time.
This means that listeners receiving log messages can be ended and restarted at will, and the subscriptions can be changed
while the Constellation is running undisturbed, enabling flexible and adaptable monitoring configurations.

More information can be found in the dedicated chapters on [logging](../concepts/logging.md) and
[telemetry](../concepts/telemetry.md).


## Limitations

Constellation targets laboratory test stands up to small and mid-sized experiments.
The focus on flexibility and ease of integrating new instruments comes with some limitations, which will be discussed in the following.

### Threat Model Considerations

In its current version, Constellation is intended to run in closed internal networks only, in the following referred to as
*subnets*. It is assumed that

* the subnet and all connected hosts can be trusted.
* there are no malicious actors on the subnet.
* the transmitted information is non-confidential to any actor on the subset.

All Constellation communication is handled exclusively via[ephemeral ports as defined in RFC 6335](https://www.iana.org/assignments/service-names-port-numbers/service-names-port-numbers.xhtml),
which is why no privileged user account is required for running a Constellation host unless the controlled hardware requires
so. If in such a case the Constellation host requires elevated privileges for running, this is clearly documented in the
respective component documentation.

It is possible to configure nodes to bind to specific network interfaces only, but the default configuration, chosen for user
convenience, is to bind to all available network interfaces of the node. In general, users are required to satisfy their
personal threat model by external means such as firewalls, physical isolation and virtualization on the edges of the subnet
Constellation runs on.

### Fixed Finite State Machine Structure

In order to maintain a simple interface for control as well as instrument integration, the available states and transitions
of the satellite FSM described in [the satellite section](./satellite.md) are fixed and cannot be extended. Furthermore, in
its current state, Constellation does not support a control hierarchy, and all satellites are directly managed by
controllers. However, future versions of the framework could implement a multi-tier control hierarchy e.g. by using group
names to separate control domains.

### Heartbeat Message Congestion

The number of heartbeat message exchanges over the CHP protocol grows quadratically with the number of active nodes.
Constellation implements an automatic congestion control that can offset performance impacts for several dozen or even
hundred nodes, large node counts however may impair the response times to failure.
