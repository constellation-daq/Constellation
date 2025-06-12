# Autonomous Operation

```{figure} CHP.svg
Schematic drawing of CHP
```

Satellites in a Constellation operate autonomously. This requires additional information on how to react in case of
failures or unforeseen situations. Following the overall decentralized approach of Constellation, this information is
provided by each satellite via their heartbeats. This means, each satellite defines independently, how the other satellites
in the Constellation should react in case if problems.

A connected satellite can convey information by

* Sending a heartbeat
* Stop sending heartbeats
* Transmitting a state
* Departing

Roles group the actions taken by the Constellation in each of these cases into an easily-configurable parameter. The role
is configured for the *sending* satellite and is transmitted via [CHP](../../protocols/chp.md) message flags to the receivers throughout the
Constellation.

## Flags

* `DENY_DEPARTURE`
* `TRIGGER_INTERRUPT`
* `MARK_DEGRADED`

## Roles

* From an `ESSENTIAL` satellite, `DENY_DEPARTURE | TRIGGER_INTERRUPT | MARK_DEGRADED`
  * reported {bdg-secondary}`ERROR` or {bdg-secondary}`SAFE` state triggers interrupt
  * heartbeat timeout triggers interrupt
  * orderly departure triggers interrupt in {bdg-secondary}`ORBIT` and {bdg-secondary}`RUN`
  * run marked as *degraded*

* From a `DYNAMIC` satellite, `TRIGGER_INTERRUPT | MARK_DEGRADED`
  * reported {bdg-secondary}`ERROR` or {bdg-secondary}`SAFE` state triggers interrupt
  * heartbeat timeout triggers interrupt
  * orderly departure allowed, logs an info
  * run marked as *degraded*

* From a `TRANSIENT` satellite, `MARK_DEGRADED`
  * reported {bdg-secondary}`ERROR` or {bdg-secondary}`SAFE` state logs a warning
  * heartbeat timeout logs a warning
  * orderly departure allowed, logs an info
  * run marked as *degraded*

* From a `NONE` satellite, flags are `NONE`
  * reported {bdg-secondary}`ERROR` or {bdg-secondary}`SAFE` state logs a warning
  * heartbeat timeout logs a warning
  * orderly departure allowed, logs an info
  * run remains does not get marked as *degraded*
