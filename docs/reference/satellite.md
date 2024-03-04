# Satellite Implementation Guidelines

## FSM States

The state can be encoded in a single-byte value.

State values with the lower four bits set to zero indicate steady states. For state values with non-zero lower four bits, the higher four bits indicate the steady state they enter into.

The following states are defined:

* `0x00` - NEW
* `0x10` - INIT
* `0x11` - initializing
* `0x12` - landing
* `0x20` - ORBIT
* `0x21` - launching
* `0x22` - reconfiguring
* `0x23` - stopping
* `0x30` - RUN
* `0x31` - starting
* `0xE0` - SAFE
* `0xE1` - interrupting
* `0xF0` - ERROR
