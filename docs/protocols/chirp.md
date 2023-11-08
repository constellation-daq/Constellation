# Constellation Host Identification and Reconnaissance Protocol

* Status: draft
* Editor: The Constellation authors

The Constellation Host Identification and Reconnaissance Protocol (CHIRP) defines how different hosts announce their services and connect to each other on the network.

## Preamble

The key words “MUST”, “MUST NOT”, “REQUIRED”, “SHALL”, “SHALL NOT”, “SHOULD”, “SHOULD NOT”, “RECOMMENDED”, “MAY”, and “OPTIONAL” in this document are to be interpreted as described in [RFC 2119](http://tools.ietf.org/html/rfc2119).

## Goals

The CHIRP protocol provides a way of discovering and announcing services on a network segment and provide the means for hosts to connect over peer-to-peer connections. The goals are:

* To work with no centralized services or mediation except those available by default on a network.
* To allow service discovery both for late-joining clients and for lingering clients when a service provider host appears late.
* To facilitate the exchange of connectivity information for different services and the ability to select distinguish between them.

## Implementation

### Identification and Life-cycle

A CHIRP host represents a source or a target for messaging. Hosts usually map to applications. A CHIRP host is identified by a 16-octet universally unique identifier (UUID). CHIRP does not define how a host is created or destroyed but does assume that hosts have a certain durability.

### Host Discovery and Service Announcement

CHIRP uses UDP IPv4 beacon broadcasts to discover hosts. Each CHIRP host SHALL listen to the CHIRP discovery service which is UDP port 7123. Each CHIRP host SHALL broadcast, upon creation, on UDP port 7123 a beacon that identifies itself to any listening hosts on the network. A separate CHIRP beacon SHALL be broadcast for every service the host advertizes.

The CHIRP beacon consists of one 27-octet UDP message with this format:

```
+---+---+---+---+---+------+  +------+------------+-----------+---------+------+
| C | H | I | R | P | %x01 |  | type | group UUID | host UUID | service | port |
+---+---+---+---+---+------+  +------+------------+-----------+---------+------+

           Header                                    Body
```

The header SHALL consist of the letters ‘C’, ‘H’, ‘I’, ‘R’ and ‘P’, followed by the beacon version number, which SHALL be %x01.

The body SHALL consist of the one-byte beacon type identifier, followed by the 16-octet UUID of the sender group, the 16-octet UUID of the sender host, a one-byte service descriptor, and a two-byte port number in network byte order. If the port is non-zero this signals that the peer will accept ZeroMQ TCP connections on that port number.

The type SHALL be either %x01 (dubbed ‘REQUEST’), %x02 (dubbed ‘OFFER’), %x03 (dubbed ‘DEPART‘).

A valid beacon SHALL use a recognized header and a body of the correct size. A host that receives an invalid beacon SHALL discard it silently. A host MAY log the sender IP address for the purposes of debugging. A host SHALL discard beacons that it receives from itself. A host SHALL discard beacons that it receives from hosts with a sender group different from its own sender group.

When a CHIRP host receives a beacon of type ‘OFFER’ from a host with the same sender group that it does not already know about, with a non-zero port number, it MAY connect to the service provided by this peer on the provided port if it SHOULD participate in the offered service.

When a CHIRP host receives a beacon with type ‘REQUEST’ from any host with the same sender group, with a zero or non-zero port number, and it offers the requested service, it MUST respond with a CHIRP beacon of type ‘OFFER’ for the requested service, providing the port number for this service.

When a CHIRP host receives a beacon of type ‘DEPART‘ from a known host with the same sender group, with a non-zero port number, it SHALL disconnect from the service offered by this peer on the provided port number. A host SHALL discard beacons of type ‘DEPART‘ from unknown hosts.

## Protocol Grammar


```abnf
chirp   = [request] *offer

; Request offers from other hosts
request = header version uuid %x01 service port

; Make an offer of a service to other hosts
offer   = header version uuid %x02 servoce port

; Header and version of the protocol
header = "chirp"
version = %x01

; Unique identifier for the host
uuid = 16OCTET

; Service definition for this beacon
service = 2OCTET

; Port of the host to connect to for this service
port = 2OCTET
```
