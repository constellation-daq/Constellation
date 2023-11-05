# Constellation CHIRP library

Implementation of the CHIRP protocol draft.

## Requirements

- Meson 1.0.0 or newer
- GCC 12 or newer

Optional:
- Gcovr, for coverage report

## Building

Build tools in `builddir` directory:
```sh
meson setup builddir
meson compile -C builddir
```

## Unit tests

First build in a new directory with coverage enabled:
```sh
meson setup builddir_coverage -Db_coverage=true
meson compile -C builddir_coverage
```

Then run tests and create coverage report:
```sh
meson test -C builddir_coverage
ninja -C builddir coverage-html
```

## Notes on sockets

Since CHIRP requires a fixed port and we might have multiple programs running CHIRP on one machine, it is important to ensure that the port is not blocked by one program. Networking libraries like ZeroMQ and NNG do this by default when binding to a wildcard address. To ensure that a socket can be used by more than one program, the `SO_REUSEADDR` socket option has to be enabled. Further, to send broadcasts the `SO_BROADCAST` socket option has to be enabled.

## Notes on broadcast addresses

CHIRP uses an UDP brodcast over port 7123, which means that it send a message to all participants in a network. However, "all" in this context depends the broadcast address.

For example, if you have a device with a fixed IP (e.g. 192.168.1.17) in a subnet (e.g. 255.255.255.0), the general broadcast address (255.255.255.255) does not work. Instead, the broadcast address for the specified subnet has to be used (e.g. 192.168.1.255). On Linux, the broadcast IP for a specific network interface can found for example by running `ip a`, it is the IP shown after `brd`.

To opposite to the broadcast address is the "any" address, which accepts incoming traffic from any IP. In general it can be deduced from the broadcast address by replacing all 255s with 0s. However, the default any address (0.0.0.0) is enough since message filtering has to be done anyway.

If no network (with DHCP) is avaible, the default broadcast address (255.255.255.255) does not work. As a workaround, the default any address (0.0.0.0) can be used to broadcast over localhost.

TODO:
- [ ] Test if default broadcast IP (255.255.255.255) works with DHCP
- [ ] Look up if it is possible to find the broadcast IP from network interface platform independently

## Broadcasting

To receive broadcasts:
```sh
./builddir/CHIRP/test/broadcast_recv
```

To send broadcasts:
```sh
./builddir/CHIRP/test/broadcast_send [BRD_IP]
```

The broadcast IP can be specified optionally. See [above](#notes-on-broadcast-addresses) for details.

## CHIRP Messages

To receive CHIRP messages:
```sh
./builddir/CHIRP/test/chirp_recv
```

To send CHIRP messages:
```sh
./builddir/CHIRP/test/chirp_send [BRD_IP]
```

The broadcast IP can be specified optionally. See [above](#notes-on-broadcast-addresses) for details.

TODO:
- [ ] Mention MD5 hashing and service identifiers somewhere - in CHIRP itself or other RFC?

Note regarding unsubscribing: sending OFFER with port 0 is not optimal, because might have multiple services on one satellite with different ports. Let us to add a new message type instead instead:
- REQUEST always port == 0
- OFFER   never  port == 0
- LEAVING never  port == 0
