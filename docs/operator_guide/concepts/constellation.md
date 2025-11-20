# System Architecture

This section of the manual provides an overview of the concepts and system architecture of the Constellation framework.

Constellation is a network-distributed control and data acquisition framework geared towards flexibility and easy integration
of new sensors, detectors and other devices. The components of the framework run on a single computer or multiple machines
across a local IP-based network, and allow to distribute commands, record data and collect monitoring and logging information
from its different constituents.

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
