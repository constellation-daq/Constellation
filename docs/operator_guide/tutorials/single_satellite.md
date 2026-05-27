# Starting & Controlling a Satellite

The following tutorial describes how to start a single [satellite](../concepts/satellite.md), how to discover it and change
its state with a command line controller. For this purposes example satellites are used, a full list of satellites can be
found under [Satellites](../../satellites/index.md).

```{hint}
This tutorial assumes that both the C++ and Python implementations of Constellation are installed. If the C++ implementation
is not installed, all appearances of `SatelliteSputnik` can be replaced with `SatelliteMariner`.
```

## Starting a Satellite

The `SatelliteSputnik` executable is used to start the example `Sputnik` satellite. It has two relevant command line
arguments:

- `--name, -n`: This is the name for satellite, which should be unique within the Constellation group (defaults to the host name of the PC).
- `--group, -g`: This is the name of the Constellation group this satellite should be a part of.

```sh
SatelliteSputnik -n One -g edda
```

```{note}
When a satellite is started this way, it runs in the foreground until it is terminated. This means that any other satellites
or commands need to be a run in a new terminal.
```

A detailed description of all available command line arguments can be found in the [Satellite](../concepts/satellite.md#the-satellite-executable) section.

## Controlling the Satellite

A controller running in the same Constellation group is needed in order to control the satellite started in the first part
of this tutorial. This section shows different options to perform this task.

```{seealso}
The [IPython Controller tutorial](./ipython_controller.md) describes the use of the interactive command line controller used hereafter in more detail.
More details on controllers in general can be found in the [Controller concepts](../concepts/controller.md) section.
```

### Starting a Controller

The Python implementation of Constellation provides a powerful command line interface controller using IPython.
This can be installed with the `cli` component:

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

The controller can be started via the command `Controller`. It has one relevant command line argument:

- `--group, -g`: This is the name of the Constellation group this controller should be a part of.

To control the satellite created in the first part of this tutorial, the controller needs to be in the same group.

```sh
Controller -g edda
```

The interactive command line provides the `constellation` object which holds all information about connected satellites and
allows their control. Getting a dictionary containing the satellites could e.g. be performed by running:

```python
edda > constellation.satellites
{'Sputnik.One': SatelliteCommLink(type=Sputnik, name=One)}
```

In order to obtain more information on a specific satellite, it can be directly addressed via its type and name
and a command can be sent. The response is then printed on the terminal:

```python
edda > constellation.Sputnik.One.get_name()
SatelliteResponse(msg='Sputnik.One')
```

The controller supports tab completion, and suggestions for possible commands are displayed by typing
`constellation.Sputnik.One.` and hitting {kbd}`Tab`.

### Sending Commands to the Satellite

Commands can either be sent to individual satellites, all satellites of a given type, or the entire constellation.
All available commands for the `constellation` object are available via tab completion.

To initialize the satellite, it needs to be sent an initialize command, with a configuration as an argument.
In the following example, the configuration is an empty dictionary (`{}`) and directly passed to the command.

```python
edda > constellation.Sputnik.One.initialize({})
SatelliteResponse(msg='Transition initialize is being initiated')
```

All satellites can be initialized together by sending the command to the entire constellation:

```python
edda > constellation.initialize({})
{'Sputnik.One': SatelliteResponse(msg='Transition initialize is being initiated')}
```

Whether the satellites has actually changed its state can be checked by retrieving the current state via:

```python
edda > constellation.get_state()
{'Sputnik.One': SatelliteResponse(
                   msg='INIT', payload=32,
                   meta={"last_changed": datetime.datetime(2025, 6, 19, 12, 4, 49, 594792, tzinfo=datetime.timezone.utc)})}
```

Here, the response of the satellite contain a message (`msg`) with the human-readable state name, a payload with the state
code and metadata with key-value pairs such as the time when the state changed last (`last_changed`).

Similarly, all satellite transitions can be called. A full list of available commands, along with a description of the finite
state machine can be found in the [concepts chapter on satellites](../concepts/satellite.md).

### Loading a Configuration File

Constellation configuration files are [TOML](https://toml.io/) files with the configuration key-value pairs for all
satellites. The individual satellite configurations are sent to their satellites together with the `initialize` command as
dictionary. Their basic structure and syntax is the following:

```toml
# General settings which apply to all satellites
[_default]
_autonomy.role = "DYNAMIC"

[Sputnik._default]
# Settings which apply to all satellites of type "Sputnik"
interval = 1000

[Sputnik.One]
# Settings which only apply to the satellite with name "Sputnik.One"
launch_delay = 1
```

When starting the controller, a configuration file can be passed as optional command line argument:

```sh
Controller -g edda --config myconfiguration.toml
```

The configuration is then available as `cfg` object on the interactive command line and can be passed to the `initialize`
function:

```python
edda > constellation.initialize(cfg)
{'Sputnik.One': SatelliteResponse(msg='Transition initialize is being initiated')}
```

Alternatively, the configuration can be read and parsed in an already running interactive command line session using the
`load_config` function. The resulting object can be directly passed to the `initialize` method:

```python
edda > cfg = load_config("myconfiguration.toml")
edda > constellation.initialize(cfg)
{'Sputnik.One': SatelliteResponse(msg='Transition initialize is being initiated')}
```

### Closing the Controller

Controllers in Constellation do not possess state and can be closed and restarted at the discretion of the user without
affecting the state of the satellites. When closing and starting the controller again, the satellites will still be in the
{bdg-secondary}`INIT` state from before.

The IPython CLI controller can be disconnected from the constellation using the command `quit` or by pressing {kbd}`Control-d` twice.
