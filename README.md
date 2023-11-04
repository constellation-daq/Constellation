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

## Broadcasting

To receive broadcasts:
```sh
./builddir/CHIRP/test/broadcast_recv
```

To send broadcasts:
```sh
./builddir/CHIRP/test/broadcast_send [BRD_IP]
```

The broadcast IP can be specified optionally. The broadcast IP for a specific network interface can found for example by running `ip a`, it is the IP shown after `brd`.

TODO:
- [ ] Test if default broadcast IP (255.255.255.255) works with DHCP
- [ ] Test if broadcast over 0.0.0.0 works without network interface
- [ ] Look up if it is possible to find the broadcast IP from network interface platoform independently

## CHIRP Messages

To receive CHIRP messages:
```sh
./builddir/CHIRP/test/chirp_recv
```

To send CHIRP messages:
```sh
./builddir/CHIRP/test/chirp_send [BRD_IP]
```

TODO:
- [ ] Mention MD5 hashing and service identifiers somewhere - in CHIRP itself or other RFC?
