# Protocols & Communication Channels

Constellation is built around a set of communication protocols among its constituents. These protocols are well-defined and have been defined
early on and serve as platform- and implementation-independent architecture of the framework, meaning that new implementations of e.g. a satellite can be written in any language.
The communication channels are independent of each other and follow clear communication patterns such as publish/subscribe for one-to-many distribution of information
or request/reply for a client-server-based communication.

Most of the protocols are TCP/IP communication based on the [ZeroMQ messaging library](https://zeromq.org/) and build upon the ZeroMQ Message Transport Protocol.
The protocols are documented in RFC-style documents, including an [ABNF description](https://en.wikipedia.org/wiki/Augmented_Backus%E2%80%93Naur_form)
where relevant, and can be found in the appendix of this manual.

The five Constellation communication protocols are described in the following, ordered by significance of the information passed.

## Heartbeating

## Command & Controlling

## Data Transmission

## Monitoring

## Network Discovery

A common nuisance in volatile networking environments with devices appearing and disappearing is the discovery of available devices and services.
While some established protocols exist for the purpose of finding services on a local network, such as zeroconf or avahi, these come with significant
downsides such as missing standard implementations, being limited to individual platforms, or a large and complex set of features not required for the
purpose of Constellation.

Hence, the Constellation Host Identification & Reconnaissance Protocol (CHIRP) has been devised which uses a set of defined beacons sent as
broadcasts over UDP/IP to announce or request services. The beacon message contains a unique identified for the host and its Constellation group,
the relevant service as well as IP address and port of the service. Three such beacons exist:

* `OFFER`: A beacon of this type indicates that the sending host is offering the service at the provided endpoint.
* `REQUEST`: This beacon solicits offers of the respective service from other hosts.
* `DEPART`: A departing beacon is sent when a host ceases to offer the respective service.

Each service the participating Constellation host offers is registered with its CHIRP service. Upon startup of the program, a `OFFER` beacon is sent
for each of the registered services.
The `REQUEST` beacon allows hosts to join late, i.e. after the initial `OFFER` beacons have been distributed. This means that at any time of the
framework operation, new hosts can join and request information on a particular service from the already running COnstellation participants.
A clean shutdown of services is possible with the `DEPART` beacon which will prompt other hosts to disconnect.
