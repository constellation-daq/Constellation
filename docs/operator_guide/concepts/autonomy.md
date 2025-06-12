# Autonomous Operation

Constellations operate decentralized and each satellite runs autonomously and no central server which manages a common state
is required. Controllers and listeners constitute the user interfaces and are entirely stateless.

The communication between the individual components of a Constellation is performed through so-called heartbeat messages
emitted by the satellites. These messages contain the finite state machine state and a set of flags which define the role of
the sending satellite as explained below.
In addition to regular heartbeat messages, satellites emit so-called extrasystoles whenever their FSM state has changed.
This ensures timely information of all Constellation constituents about new states or errors.

The interval between the regular heartbeat messages is automatically scaled according to the number of active hosts in the
Constellation to avoid congestion.

```{figure} CHP.svg
Schematic drawing of heartbeating in Constellation
```

## Satellite Autonomy & Roles

Satellites in a Constellation operate autonomously. This requires additional information on how to react in case of
failures or unforeseen situations. Following the decentralized approach of Constellation, this information is
provided by each satellite via their heartbeats. Each satellite defines independently how other satellites in the
Constellation should react in case of problems.

Satellites can convey information to the Constellation by a number of possibilities:

* The satellite can send a heartbeat message
* The satellite can stop sending heartbeats, e.g. because of a network issue or because the satellite program exited abnormally
* The satellite transmits its state through heartbeat messages
* The satellite can depart by informing the Constellation about the unavailability of its services

Roles group the actions taken by the Constellation in each of these cases into an easily-configurable parameter. The role
is configured for the *sending* satellite and is transmitted via heartbeat message flags to the receivers throughout the
Constellation. The following roles exist and can be configured through the `_role` parameter:

* `ESSENTIAL` satellites are considered key to the running Constellation. Their information is treated most strictly, in
  particular
  * any reported {bdg-secondary}`ERROR` or {bdg-secondary}`SAFE` state triggers an interrupt in other satellites
  * a timeout in receiving heartbeats from an `ESSENTIAL` satellite triggers an interrupt in other satellites
  * an orderly departure of a `ESSENTIAL` satellite triggers an interrupt in {bdg-secondary}`ORBIT` and {bdg-secondary}`RUN` states
  * any of the above will lead to the current run being marked with the *degraded* flag.

* `DYNAMIC` satellites are the default constituents, where
  * any reported {bdg-secondary}`ERROR` or {bdg-secondary}`SAFE` state triggers an interrupt
  * a timeout in receiving heartbeats triggers an interrupt in other satellites
  * an orderly departure of an `DYNAMIC` satellite **is allowed** and only an information about the departure is logged
  * any of the above will lead to the current run being marked with the *degraded* flag.

* `TRANSIENT` satellites are more volatile components in a Constellation, namely
  * any reported {bdg-secondary}`ERROR` or {bdg-secondary}`SAFE` state **only logs a warning** but does not trigger an interrupt
  * a timeout in receiving heartbeat messages **logs a warning**
  * an orderly departure of an `TRANSIENT` satellite **is allowed** and only an information about the departure is logged
  * any of the above will lead to the current run being marked with the *degraded* flag.

* Satellites with the role `NONE` are mere spectators,
  * any reported {bdg-secondary}`ERROR` or {bdg-secondary}`SAFE` state **only logs a warning**
  * a timeout in receiving heartbeat messages **logs a warning**
  * an orderly departure of an `NONE` satellite **is allowed** and only an information about the departure is logged
  * The run metadata flags of the current run are not affected by this satellite.

The default role of satellites is the `DYNAMIC` role, the currently configured role can be requested with the `get_role`
command.
