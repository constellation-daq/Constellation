# Networking & Discovery

This section introduces a few details on the networking aspects of Constellation and the ramifications on operations.

## Network Discovery via *CHIRP*

Constellation implements the *CHIRP* protocol to find other nodes on the same network.
This avoids having to hard-code IP addresses of every node in configuration files, and enables dynamic setups where components
such as listeners or controllers can join and leave the Constellation at any time.

*CHIRP* is an IPv4 protocol intended to be used on local networks only, which uses a set of defined beacon UDP messages to announce
or request services.
These messages are distributed as so-called *multicast messages* via UDP port 7123 to the multicast address `239.192.7.123`.

When a satellite, controller or listener joins the Constellation, it will send a `OFFER` beacon for every protocol service it
offers. For a satellite, this implies sending offers for control, telemetry and heartbeats.
In addition, the joining node can send `REQUEST` beacons to cause all other nodes to resend their offer messages for the
requested service.
Finally, when services are shut down, the a `DEPART` beacon will prompt other nodes to disconnect.

```{seealso}
A detailed technical description, including protocol sequence diagrams, can be found in the
[protocol description chapter](../../framework_reference/protocols.md#network-discovery) in the framework development guide.
```

## Ephemeral Ports & Firewalls

Some operating systems come with automatically enabled firewalls and a relatively strict rule set. It might be necessary to
adjust the firewall configuration to allow Constellation messages to pass. This concerns the following settings:

*CHIRP* communicates over a fixed UDP port as described above. Accordingly, **firewalls must allow incoming and outgoing UDP packets on port 7123**.

All protocols apart from *CHIRP* operate via TCP and use [ephemeral ports](https://en.wikipedia.org/wiki/Ephemeral_port).
These ports are meant to be used for short amounts of time, i.e. the duration of a communication session and usually do not
require root or superuser privileges to be used. They are automatically assigned by the operating system at startup of the service.

On the one hand, this means that Constellation nodes can be started from any regular user account
that has access to the relevant network interfaces.
On the other hand, communication ports change every time a Constellation node is stopped and started again, and the newly assigned ports are communicated again via *CHIRP*.
Thus, **firewalls must allow incoming and outgoing TCP packets on ephemeral ports**.

A detailed description on how to configure firewalls on different operating systems can be found in the How-To Guide on [Configuring Firewalls](../howtos/firewalls.md).
