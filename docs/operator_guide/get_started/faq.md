# Frequently Asked Questions

This is a list of frequently asked questions and commonly encountered issues when setting up and operating a Constellation.

## Starting Constellation Nodes

:::{dropdown} Satellites are not discovered / do not show up in controller
When satellites are not discovered or do not show up in controller interfaces such as MissionControl, the reason likely is an
issue in the network communication:

* The machines are not **on the same subnet**. The `ping` command can be used to test connection between machines.
* The satellite is in a **different Constellation group**. Groups are explained in the [Systems Architecture section](../concepts/constellation.md#network-discovery).
* A **firewall is blocking** incoming UDP or TCP packets. The [Configuring Firewalls](../howtos/firewalls.md) How-To Guide describes in detail which settings of the firewall need to be adjusted.
* The communication is **routed over the wrong network** interface. When starting Constellation nodes, the `-i`/`--interface` command line argument can be used to restrict communication to one or several network interfaces. Command line arguments are documented in the [Satellite section](../concepts/satellite.md#the-satellite-executable).
:::

:::{dropdown} Satellites on macOS cannot access the network
:::

## Configuration

:::{dropdown} MissionControl warns about satellites not found in configuration file
MissionControl issues a warning when it discovers satellites in the Constellation group that have no corresponding entry in
the loaded configuration file. This is a guard against typos: a misspelled satellite name would otherwise silently receive
an empty configuration.

If the satellite in question does indeed not need any configuration parameters, this warning can be safely ignored. The
message can be silenced by adding an explicit empty section for it in the configuration file:

```toml
[Sputnik.Three]
```

If the satellite was not expected to appear, it should be checked that its Constellation group name matches and that it is
not a leftover process from a previous session or supposed to be part of a different Constellation.
:::

:::{dropdown} An environment variable placeholder is not resolved or causes an error
If Constellation reports that an environment variable cannot be found, or the placeholder appears
literally in the final value, the following items should be checked:

* **Controller-side vs. satellite-side syntax.** Variables written as `${VARIABLE}` are resolved on the satellite host at the
  time the configuration key is first accessed by the satellite. Variables written as `$_{VARIABLE}` are resolved by the
  controller before the configuration is sent to individual satellites, and has to be present on the controller machine.
* The **variable is not set** in the environment. It should be ensured, e.g. by running `echo $VARIABLE` in the same shell as
  the node in question, that the variable in question is exported to the environment. Shell variables defined without the
  `export` keyword are not visible to child processes.
* A **default fallback can be specified** using the `:-` syntax: `${VARIABLE:-fallback}`. This prevents errors for absent
  variables and is especially useful during initial setup of a Constellation.

The [Configuration Files](../concepts/configuration_files.md#environment-variables) concept section describes the full
available syntax for parsing environment variables.
:::

:::{dropdown} Reconfiguration of parameters in configuration section fails
Configuration sections behave like nested Python dictionaries, and when attempting to reconfigure a parameter in a section,
for example when using the [scriptable controller](../concepts/controller.md#scriptable-controller), noting the key
hierarchy using the dot notation known from configuration files in the TOML format will lead to errors:

```python
constellation.MySatellite.One.reconfigure({'devices.ADC.registers.threshold': 123})
```

The issue is that the enclosing quotes mark the entire key as single string, and it is parsed and transmitted as such. In
order to reproduce the nested section structure of the configuration, the parameter to be reconfigured has to be provided
using this full structure. For the above example, this means writing:

```python
constellation.MySatellite.One.reconfigure({'devices': {'ADC': {'registers': {'threshold': 123}}}})
```

More information on section syntax can be found in the concepts section on [Configuration Files](../concepts/configuration_files.md#sections).
:::

## State Transitions and Errors

:::{dropdown} A satellite is stuck in a transitional state
Satellites execute their configuration or hardware setup and communication in transitional states of their finite state
machine ({bdg-secondary}`initializing`, {bdg-secondary}`launching`, {bdg-secondary}`landing`, {bdg-secondary}`starting`,
{bdg-secondary}`stopping`, {bdg-secondary}`reconfiguring`). If a satellite remains in one of these states longer than
expected, the following possible causes should be checked:

* A **conditional transition is waiting** for another satellite. [Conditional transitions](../concepts/autonomy.md#conditional-transitions)
  enable satellites to depend on the successful transitions of remote satellites. If the parameter
  `_conditions.require_<state>_after` has been configured, the satellite waits until the listed remote satellites have
  completed the corresponding transition. If any of the listed remote satellites enters {bdg-secondary}`ERROR` state, the
  condition is aborted. If a remote satellite is in a different state and will not reach conclude the configured transition,
  the waiting satellite will eventually time out and enter {bdg-secondary}`ERROR`. The default timeout is set to 30 seconds,
  and can be adjusted with the `_conditions.transition_timeout` parameter.
* The instrument **hardware is slow** to respond or a slow procedure is carried out by the satellite. Some hardware such as
  high-voltage power supplies can require significant time to ramp during {bdg-secondary}`launching` or {bdg-secondary}`landing`.
  Usually, satellites communicate such procedures via their log messages.
* A **hardware communication failure** occurred. While in most cases, a hardware communication failure will trigger a
  transition into the {bdg-secondary}`ERROR` state, it can happen that hardware does not respond anymore and the satellite
  remains waiting for an answer without a programmed timeout. In these cases the issue should be identified and the satellite
  code optimized to time out correctly when communication breaks down.
:::

## Logging and Monitoring

:::{dropdown} Observatory shows no log messages
If Observatory connects to the Constellation but displays no log messages even when there is activity in the Constellation,
such as satellite finite state machine transitions, this could have the following reasons:

* The **global log level is set too high.** If the global subscription level is `CRITICAL`, only `CRITICAL` messages will be
  received. A lower status such as `WARNING` or `INFO` is recommended in order to see routine operation messages.
* A **message filter prevents display** of the relevant messages. Message filters are temporary filters to aid searching in
  the already received messages. All filters can be reset using the {bdg-primary}`Reset` button in the top bar.
* Observatory was **started after the activity** occurred. Any message emitted before the Observatory instance connected to
  the Constellation are not resent retroactively, only messages sent after connecting and subscribing to the desired log
  topics are transmitted and displayed by the Observatory.

A detailed tutorial on using the Observatory is available in the [Tutorials section](../tutorials/observatory.md).
:::

## Scripted Controller

:::{dropdown} Controller script fails with timeout in `await_state`
Either a transition of a connected satellite took longer than time timeout for the `await_state` call, or one of the
satellites has not received the transition command leading to the awaited state.

The former could for example happen when slowly ramping an output voltage, and the ramp duration exceeds the default timeout
of 60 seconds. The solution is to ensure sufficient time is given to all satellites to conclude their transition by passing
and explicit timeout to the function that is adapted to the expected transition time:

```python
ctrl.await_state(SatelliteState.ORBIT, 120)
```

The latter situation can occur when a controller script is started, and a command is sent before all satellites
have connected:

```python
ctrl = ScriptableController(group_name)
ctrl.constellation.start("new_run")
ctrl.await_state(SatelliteState.RUN)
```

This creates a race condition where not all satellites were connected when the `start` command was sent, and they consequently
remain in {bdg-secondary}`ORBIT` state. It is therefore strongly recommended to ensure that all satellites of the
Constellation have connected to the controller before sending commands. The controller class provides helper
functions for this purpose:

```python
# Wait until all listed satellites are connected
ctrl.await_satellites(["Sputnik.One", "Mariner.Nine"])

# Alternatively, wait until a certain number of satellites has connected
ctrl.await_n_satellites(2)
```

Example scripts can be found in the How-To Guide on [Parameter Scans with Python](../howtos/scanning_python.md).
:::
