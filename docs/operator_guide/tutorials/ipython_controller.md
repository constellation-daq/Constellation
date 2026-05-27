# The IPython Controller

Constellation ships with an interactive command-line controller built on
[IPython](https://ipython.org/). It gives operators a full Python environment from which they can
discover satellites, inspect their state, send commands, load configuration files, emit log messages
into the shared log stream, and run ad-hoc Python expressions — all without writing a script first.

This tutorial walks through a complete session: starting a small Constellation, initializing and
launching it, taking a run, and recovering from a simple error. It assumes that the
[`SatelliteSputnik` satellite has been started](./single_satellite.md) and that the reader is
familiar with the [Satellite concept](../concepts/satellite.md).

```{seealso}
The [MissionControl](./missioncontrol.md) controller covers the same fundamental
workflow through a graphical user interface.
For automated, script-driven operation — running without an interactive session — the `ScriptableController` can be used for
example to set up [Parameter Scans with Python](../howtos/scanning_python.md).
```

## Starting the Controller

The interactive controller is provided by the `cli` extra of the Constellation Python package and can be installed via:

::::{tab-set}
:::{tab-item} PyPI
:sync: pypi

```sh
pip install "ConstellationDAQ[cli]"
```

:::
:::{tab-item} Source
:sync: source

```sh
pip install --no-build-isolation -e ".[cli]"
```

:::
::::

Once installed, it is launched with the `Controller` command. The only required argument is the
Constellation group name, which must match the group the satellites belong to:

```sh
Controller -g edda
```

Optionally, the controller can be given a name to identify it e.g. in log messages emitted to the Constellation:

```sh
Controller -g edda -n mycontroller
```

By default, the controller uses the host name of the machine it is started on.

An optional configuration file can be supplied directly at startup, making it available as the
`cfg` object inside the session as covered in detail [below](#loading-a-configuration):

```sh
Controller -g edda --config myconfiguration.toml
```

At startup, the controller connects to the running Constellation and displays the IPython prompt.

```text
|2026-05-09 14:43:05| INFO     [LINK] Using interfaces addresses ['127.0.0.1']
|2026-05-09 14:43:06| INFO     [LINK] Registered heartbeating check for tcp://127.0.0.1:45951
|2026-05-09 14:43:06| INFO     [BASECONTROLLER] Satellite Sputnik.One connected

Welcome to the Constellation CLI IPython Controller!

You can interact with the discovered Satellites via the `constellation` array:
         > constellation.get_state()

To get help for any of its methods, call it with a question mark:
         > constellation.get_state?

   Happy hacking :)

Starting IPython Controller for Constellation
📡 v0.8.1 (Caelum) 🛰 2 😀 NEW ipython
edda >
```

The status line displays the Constellation version of the controller, the number of connected satellites, and the current
global state of the Constellation along with descriptive emojis. The Constellation group
name is shown as the prompt prefix and identifies which Constellation is being controlled.

Two objects are pre-defined and available in every session:

- `constellation` — represents the Constellation and provides access to commands that can be either sent individually to each
  of the discovered satellite, to a certain type of satellite or to the entire Constellation.
- `ctrl` — the underlying Python controller instance. It provides utility methods such as
  `await_state`, which pauses execution until all satellites have reached a target state.

```{note}
The controller is stateless. It can be closed and restarted at any time without affecting the
satellites. When it reconnects, it rediscovers the running Constellation automatically and displays their current FSM states
as well as the last active run identifier.
```

## Discovering Satellites

As soon as the controller is running it begins listening for [CHIRP discovery messages](../concepts/discovery.md) and builds a
live map of the satellites in the Constellation. This map is always accessible via
`constellation.satellites`:

```python
edda > constellation.satellites
{'Sputnik.One': SatelliteCommLink(type=Sputnik, name=One),
 'Sputnik.Two': SatelliteCommLink(type=Sputnik, name=Two)}
```

The dictionary keys are canonical names in the form `Type.Name`. The controller updates this map
whenever satellites join or leave the group, so the view is always current.

Individual satellites can be addressed directly by their canonical name using attribute access. Tab
completion works throughout, so pressing {kbd}`Tab` after `constellation.` displays all available commands and discovered
satellite, pressing {kbd}`Tab` after `constellation.Sputnik.` lists the commands and the discovered satellites of this type,
and pressing {kbd}`Tab` after `constellation.Sputnik.One.` lists every command that particular satellite exposes:

```python
edda > constellation.Sputnik.One.get_name()
SatelliteResponse(msg='Sputnik.One')

edda > constellation.Sputnik.One.get_state()
edda > constellation.Sputnik.One.get_state()
SatelliteResponse(
                   msg='NEW', payload=16,
                   meta={"last_changed": datetime.datetime(2026, 5, 9, 12, 42, 52, 329643, tzinfo=datetime.timezone.utc)})

```

Every command returns a `SatelliteResponse` object. Its most useful fields are:

- `msg` — a human-readable string, typically the state name or an acknowledgment message.
- `payload` — the machine-readable data accompanying the response (for example the state encoded as an integer or
  a configuration dictionary). The contents depend on the command.
- `meta` — a dictionary of additional key-value pairs, such as `last_changed`, the timestamp when
  the satellite last changed state.

The same commands can be issued to all satellites at once by calling them on `constellation`
directly. The return value is then a dictionary mapping each canonical name to its individual `SatelliteResponse`:

```python
edda > constellation.get_state()
{'Sputnik.One': SatelliteResponse(msg='NEW', payload=16, meta={...}),
 'Sputnik.Two': SatelliteResponse(msg='NEW', payload=16, meta={...})}
```

All satellites of a given type can be addressed together as well, which is convenient when multiple
instances of the same type exist:

```python
edda > constellation.Sputnik.get_state()
{'Sputnik.One': SatelliteResponse(msg='NEW', payload=16, meta={...}),
 'Sputnik.Two': SatelliteResponse(msg='NEW', payload=16, meta={...})}
```

Because the session is a full Python interpreter, the standard output methods can be used to format
responses in any way that helps. The following loop, for example, prints a compact state summary:

```python
edda > for name, sat in constellation.satellites.items():
  ...:     print(f"{name:30s}  {sat.get_state().msg}")
  ...:
Sputnik.One                     NEW
Sputnik.Two                     NEW
```

## Loading a Configuration

Satellites require a configuration for their initialization. Configuration files are TOML or YAML files containing the
configuration parameters for all satellites in the Constellation. Their format are described in detail in the
[Configuration Files section](../concepts/configuration_files.md).

If a configuration file was provided on the command line at startup, it is already available via the `cfg` object.
To load a file after startup, or to switch to a different file mid-session, the `load_config` helper reads and
parses a file:

```python
edda > cfg = load_config("myconfiguration.toml")
```

The `cfg` object is a `ControllerConfiguration` whose structure mirrors the configuration file. It can be
inspected directly:

```python
edda > cfg
{'Sputnik.One': {'interval': 2500},
 'Sputnik.Two': {'interval': 3000}}
```

## Initializing and Launching

With the configuration ready, the satellites can be initialized. The `initialize` command accepts
the full `cfg` object and the controller takes care of routing each section to its  corresponding satellite:

```python
edda > constellation.initialize(cfg)
{'Sputnik.One': SatelliteResponse(msg='Transition initialize is being initiated'),
 'Sputnik.Two': SatelliteResponse(msg='Transition initialize is being initiated')}
```

The response is immediate since the satellites confirm their entering into the {bdg-secondary}`initializing` state in which
they perform their initialization work asynchronously.
Once all satellites are in the {bdg-secondary}`INIT` state, they can be launched to the {bdg-secondary}`ORBIT` state,
which is when instrument hardware is fully powered and ready for data taking:

```python
edda > constellation.launch()
{'Sputnik.One': SatelliteResponse(msg='Transition launch is being initiated'),
 'Sputnik.Two': SatelliteResponse(msg='Transition launch is being initiated')}
```

The state and status message of individual satellites described in the [Satellite chapter](../concepts/satellite.md#state-and-status) can be polled at any time:

```python
edda > constellation.Sputnik.One.get_state()
SatelliteResponse(msg='ORBIT', payload=48,
                  meta={"last_changed": datetime.datetime(2026, 5, 11, 8, 53, 55, 143613, tzinfo=datetime.timezone.utc)})
edda > constellation.Sputnik.One.get_status()
SatelliteResponse(msg='Satellite launched successfully')
```

## Starting and Stopping a Run

From the {bdg-secondary}`ORBIT` state, a run is started by providing a run identifier to the `start` command. The run
identifier is a free-form string composed of alphanumeric characters, underscores, or dashes:

```python
edda > constellation.start("run_0001")
{'Sputnik.One': SatelliteResponse(msg='Transition start is being initiated'),
 'Sputnik.Two': SatelliteResponse(msg='Transition start is being initiated')}
```

All satellites are now in the {bdg-secondary}`RUN` state and data is being acquired. The run identifier can be
confirmed at any time:

```python
edda > constellation.Sputnik.One.get_run_id()
SatelliteResponse(msg='run_0001')
```

The controller can be closed without interrupting the run. Satellites operate autonomously and continue taking data
regardless of whether a controller is connected. Reconnecting by starting a new controller with `Controller -g edda` will
rediscover the running satellites and report their current state and run identifier automatically.

To end the run, the `stop` command is issues and satellites will to return to {bdg-secondary}`ORBIT`:

```python
edda > constellation.stop()
{'Sputnik.One': SatelliteResponse(msg='Transition stop is being initiated'),
 'Sputnik.Two': SatelliteResponse(msg='Transition stop is being initiated')}
```

## Sending Commands to Individual Satellites

Sometimes it is necessary to interact with a specific satellite rather than the group as a whole.
Any satellite in the `constellation.satellites` dictionary can be addressed individually.

A useful first step with an unfamiliar satellite is to ask it what commands it supports:

```python
edda > constellation.Sputnik.One.get_commands()
SatelliteResponse(
                   msg='16 commands known, list attached in payload',
                   payload={"get_channel_reading": 'This example command reads the a device value from the channel number provided as argument. ...',
                            "get_commands": 'Get commands supported by satellite (returned in payload as flat MessagePack dict with strings as keys)',
                            "get_config": 'Get config of satellite (returned in payload as flat MessagePack dict with strings as keys)',
                            "get_name": 'Get canonical name of satellite',
                            "get_role": 'Get role of satellite',
                            "get_run_id": 'Current or last run identifier',
                            "get_state": 'Get state of satellite',
                            "get_status": 'Get status of satellite',
                            ...})
```

The currently active configuration of a satellite can be retrieved with the `get_config` command. This is especially
useful when the controller was restarted without access to the original configuration file, or when verifying that the
intended parameters were applied:

```python
edda > constellation.Sputnik.One.get_config()
SatelliteResponse(
                   msg='Configuration attached in payload',
                   payload={"_autonomy": {'max_heartbeat_interval': 30,
                             'role': 'DYNAMIC'},
                            "interval": 2500,
                            "launch_delay": 0})
```

Satellite implementations may expose additional custom commands beyond the standard set. These appear in the `get_commands`
output alongside the built-in ones, such as the `get_channel_reading` command above, and are called in exactly the same way.
Arguments to custom commands need to be provided as Python list:

```python
edda > constellation.Sputnik.One.get_channel_reading([42])
SatelliteResponse(msg='Command returned: 579.6', payload=579.6)
```

Similarly to accessing individual satellites, also all satellites of a specific type can be called via attribute access:

```python
edda > constellation.Sputnik.get_status()
{'Sputnik.Two': SatelliteResponse(msg='Satellite stopped run successfully'),
 'Sputnik.One': SatelliteResponse(msg='Satellite stopped run successfully')}
```

## Reconfiguring Without Relaunching

The {bdg-secondary}`reconfiguring` transition lets a satellite update selected parameters from the {bdg-secondary}`ORBIT`
state directly, avoiding having to land, re-initialize, and re-launch. Only satellites that explicitly implement the
{bdg-secondary}`reconfiguring` transition support this, unsupported satellites will reject the command and remain in
{bdg-secondary}`ORBIT`.

A partial configuration dictionary, containing only the keys that should change, is passed as the
argument. Unchanged parameters are left as they are:

```python
edda > constellation.Sputnik.One.reconfigure({"interval": 500})
{'Sputnik.One': SatelliteResponse(msg='Transition reconfigure is being initiated')}
```

It is also possible to reconfigure all satellites of a given type simultaneously:

```python
edda > constellation.Sputnik.reconfigure({"interval": 500})
{'Sputnik.One': SatelliteResponse(msg='Transition reconfigure is being initiated'),
 'Sputnik.Two': SatelliteResponse(msg='Transition reconfigure is being initiated')}
```

```{seealso}
The [Parameter Scans with Python](../howtos/scanning_python.md) how-to guide shows how to combine
reconfigure and start/stop in a loop to automate multi-step measurement campaigns from within the
interactive session.
```

## Handling Errors

When a satellite encounters a problem it cannot recover from automatically it enters the ERROR state.
The error details are usually available in the satellite's status message and in the log output:

```python
edda > constellation.Sputnik.One.get_state()
SatelliteResponse(msg='ERROR', payload=240, ...)

edda > constellation.Sputnik.One.get_status()
SatelliteResponse(msg='Communication timeout with device on /dev/ttyUSB0')
```

Depending on their `_autonomy.role` configuration, other satellites will follow the error into the {bdg-secondary}`SAFE`
state:

```python
edda > constellation.Sputnik.Two.get_state()
SatelliteResponse(msg='SAFE', payload=224, ...)

edda > constellation.Sputnik.Two.get_status()
SatelliteResponse(msg='Interrupting satellite operation: Sputnik.One reports state ERROR')
```

In addition, any satellite error state will color the `edda >` prompt in red.

The {bdg-secondary}`ERROR` and {bdg-secondary}`SAFE` states must be resolved by manual intervention.
After investigating and fixing the underlying cause, for example by reconnecting a device, correcting the configuration, or
restarting the hardware, the respective satellite is reset with a `initialize` call.

## Sending Log Messages

The controller participates in the Constellation logging system, described in the
[Logging & Verbosity Levels](../concepts/logging.md) section, and can therefore emit log messages that can be received
by any connected listener. This makes it possible to insert operator notes directly into the shared log stream, so that
actions taken from the interactive session are recorded alongside the satellite log output.

Log messages are sent through the `ctrl` object using `log` along with the desired log level:

```python
edda > ctrl.log.status("Starting high-voltage ramp-up sequence")
```

All messages sent this way are published under the `OP` log topic, which is the standardized topic
for operator actions. Listeners can therefore subscribe specifically to `LOG/STATUS/OP` or any other level with the `OP`
topic to follow operator activity without receiving the full satellite log output.

In listener interfaces such as [Observatory](./observatory.md), messages from the controller appear in the log display like
any satellite message, with the canonical name of the controller as the sender.

```{tip}
Log messages are a useful way to mark significant moments in a measurement campaign, for example, noting when a cable was
reconnected, when a hardware parameter was adjusted by hand, or when a run was intentionally cut short. Because they are
stored alongside satellite logs, they remain available for later inspection and can be correlated with the data.
```

## Shutting Down

Individual satellites, or all of them, can be shut down via the `shutdown` command.
Satellites only accept `shutdown` from the {bdg-secondary}`NEW`, {bdg-secondary}`INIT`, {bdg-secondary}`SAFE`, or
{bdg-secondary}`ERROR` states. A Constellation that is currently in {bdg-secondary}`ORBIT` or running a measurement must be
landed first:

```python
edda > constellation.land()
{'Sputnik.One': SatelliteResponse(msg='Transition land is being initiated'),
 'Sputnik.Two': SatelliteResponse(msg='Transition land is being initiated')}

edda > constellation.shutdown()
{'Sputnik.One': SatelliteResponse(msg='Shutting down satellite'),
 'Sputnik.Two': SatelliteResponse(msg='Shutting down satellite')}
```

The `constellation.satellites` dictionary will become empty once the satellites have exited.

## Closing the Controller

The interactive session is closed with the `quit` command or by pressing {kbd}`ctrl-d` twice. This has no effect on the
running satellites.
