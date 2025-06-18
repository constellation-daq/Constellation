# CHIRP

## Notes on Sockets

CHIRP uses [multicast](https://en.wikipedia.org/wiki/Multicast) for network discovery, meaning message are distributed to all
computers in a network which subscribed to the corresponding multicast group. A multicast group is just a specific multicast
IP address. CHIRP uses `239.192.7.123` as its multicast address and uses port `7123`.

To receive multicasts, a socket is required which binds to the multicast endpoint (which is the multicast address and port)
and joins the multicast group on each network interface. Since there might be multiple programs running CHIRP on one machine,
it is important to ensure that the port is not blocked by one program. To ensure that a socket can be used by more than one
program, the `SO_REUSEADDR` socket option has to be enabled. To receive multicast messages from other programs running CHIRP
the `IP_MULTICAST_LOOP` socket option has to be enabled.

For sending, a separate socket for each interface is required. Setting the interface is done by using the `IP_MULTICAST_IF`
option. Sending sockets also requires the `SO_REUSEADDR` socket option. Note that the `IP_MULTICAST_LOOP` option should not
be set since it is better to add the loopback interface explicitly to avoid message duplication when using multiple network
interfaces. Further it is not required to join the multicast group, sending can be achieved by sending a message to the
multicast endpoint.

Multicasts can also "hop" over network boundaries (i.e. routers) using the `IP_MULTICAST_TTL` socket option (which defines
the number of network hops). By default CHIRP uses a TTL of eight. However, routers might still be configured to drop
multicast instead of forwarding them.

## CHIRP Testing Tool

To run the CHIRP testing tool, run:

```sh
./build/cxx/constellation/tools/chirp_manager CONSTELLATION_GROUP NAME INTERFACE
```

The following commands are available:

- `list_registered_services`: list of services registered by the user in the manager
- `list_discovered_services [SERVICE]`: list of services discovered by the manager and are in the same group
- `register_service [SERVICE] [PORT]`: register a service in the manager
- `register_callback [SERVICE]`: register a discover callback for a service that prints the discovered service
- `request [SERVICE]`: send a CHIRP request for a given service
- `unregister_service [SERVICE] [PORT]`: unregister a service in the manager
- `unregister_callback [SERVICE]`: unregister a discover callback for a service
- `reset`: unregister all services and callbacks, and forget discovered services
- `quit`

## `constellation::chirp` Namespace

```{doxygennamespace} constellation::chirp
:content-only:
:members:
:protected-members:
:undoc-members:
```
