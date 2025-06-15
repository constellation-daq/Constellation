# Glossary

This page contains a brief description of commonly used terms in the context of the Constellation control & data acquisition
system.

```{glossary}
Constellation
  Either the name of the control & data acquisition software, or a group of {term}`Satellites <Satellite>`.

Satellite
  Central component of a Constellation containing a {term}`Finite State Machine` as well as communication modules to receive
  and process commands from {term}`Controllers <Controller>`.

Controller
  Component of a Constellation which can send commands to {term}`Satellites <Satellite>` and thereby control their state.
  Often controllers also receive {term}`Heartbeats` from the Constellation in order to show the most up-to-date state
  information available. Controllers are stateless and can be restarted at any time.

Finite State Machine
  Central component of any {term}`Satellite` which keeps track of the current state of the satellite and its attached
  instrument hardware, and which controls which transitions into other states are currently allowed.

Transition
  Change from one state of the {term}`Finite State Machine` to another state, triggered either by a transition command sent
  by a {term}`Controller` or by an autonomous reaction.

Telemetry
  System to send, distribute and receive monitoring information.

Transmitter
  A {term}`Satellite` which can transmit data to {term}`Receiver` satellites. These satellites usually control instruments
  which generate data. They retrieve data from the instrument data acquisition and send them to storage.

Receiver
  A {term}`Satellite` which can receive data from one or multiple {term}`Transmitter` satellites. Usually represents a
  storage note which received instrument data and saves it to disk or to a database.

Listener
  Passive component of a Constellation which can subscribe to {term}`Heartbeats`, {term}`Telemetry` or logging information
  and display them to the operator. Listeners are stateless and can be restarted at any time.

Run
  One individual measurement, characterized by a {term}`Finite State Machine` transition into and out of the
  {bdg-secondary}`RUN` state.

Measurement Queue
  Series of measurement runs mediated through a {term}`Controller`. Each of these measurements can have a different set of
  parameters for the participating {term}`Satellites <Satellite>`, which is passed to them via the
  {bdg-secondary}`reconfiguring` {term}`Transition`.

Heartbeats
  Messages distributed by {term}`Satellites <Satellite>` in regular intervals to all other components of the Constellation.
  These heartbeat messages contain the current {term}`Finite State Machine` state along with some message flags indicating
  the desired treatment of the sender.

```
