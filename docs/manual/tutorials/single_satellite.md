# Starting & Controlling a Satellite

Satellites and controllers can be implemented in either Python and in C++. Due to the shared communication protocols, a
Python controller can control a C++ satellite and vice versa. The following tutorial describes how to start a single
satellite and how to discover it and change its state with a controller.

## Starting a Satellite

::::{tab-set}
:::{tab-item} C++
:sync: cxx

The `satellite` executable starts a new Constellation satellite and requires three command line arguments:

- `--type, -t`: Type of satellite. This corresponds to the class name of the satellite implementation.
- `--name, -n`: Name for the satellite. This name is a user-chosen name and should be unique within the constellation.
- `--group, -g`: This is the name of the Constellation group this satellite should be a part of.

A call with all three parameters provided could e.g. look like follows:

```sh
./build/cxx/constellation/exec/satellite -t Sputnik -n TheFirstSatellite -g MyLabPlanet
```

:::
:::{tab-item} Python
:sync: python

Before starting a Python satellite, the virtual environment created during the installation of the framework might need to be
reactivated using `source venv/bin/activate`.

The satellite is then directly started via its Python module. Additional parameters can be supplied, such as:

- `--name, -n`. Name for the satellite. This name is a user-chosen name and should be unique within the constellation.
- `--group, -g`. This is the name of the Constellation group this satellite should be a part of.

Starting the example satellite implementation provided with the framework would therefore look like follows:

```sh
python -m constellation.satellites.example --name TheFirstSatellite --group MyLabPlanet
```

:::
::::

## Controlling the Satellite

A controller running in the same Constellation group is needed in order to control the satellite started in the first part
of this tutorial. This section shows different options to perform this task.

### Starting a Controller

::::{tab-set}
:::{tab-item} C++
:sync: cxx

TODO

:::
:::{tab-item} Python
:sync: python

The Python implementation of Constellation provides a powerful command line interface controller using IPython. This can be installed with the `cli` component (see [Installing from Source](../install.md#installing-the-constellation-package)).

The controller is started via its Python module, and it is possible to pass it some (optional) arguments, for example:

- `--name`. A name for the controller (default: cli_controller)
- `--group`. The constellation group to which the controller should belong (default: constellation)
- `--log-level`. The logging verbosity for the controller's log messages (default: info)

To control the satellite created in the first part of this tutorial, the controller needs to be in the same group.

```sh
Controller --group myLabPlanet
```

The interactive command line provides the `constellation` object which holds all information about connected satellites and
allows their control. Getting a dictionary containing the satellites could e.g. be performed by running:

```python
In [1]: constellation.satellites
Out[1]: {'Sputnik.TheFirstSatellite': <constellation.core.controller.SatelliteCommLink at 0x700590f015b0>}
```

In order to obtain more information on a specific satellite, it can be directly addressed via its type and name
and a command can be sent. The response is then printed on the terminal:

```python
In [2]: constellation.Sputnik.TheFirstSatellite.get_name()
Out[2]: {'msg': 'sputnik.thefirstsatellite', 'payload': None}
```

Since this is an interactive IPython console, of course also loops are possible and could look like this with two satellites
connected:

```python
In [3]: for sat in constellation.satellites.values():
   ...:     sat.get_name()
   ...:
{'msg': 'sputnik.thefirstsatellite', 'payload': None}
{'msg': 'sputnik.thesecondsatellite', 'payload': None}
```

:::
::::

### Sending Commands to the Satellite

::::{tab-set}
:::{tab-item} C++
:sync: cxx

TODO

:::
:::{tab-item} Python
:sync: python

Commands can either be sent to individual satellites, all satellites of a given type, or the entire constellation.
All available commands for the `constellation` object are available via tab completion.

To initialize the satellite, it needs to be sent an initialize command, with a dictionary of config options as an argument.
In the following example, this dictionary is empty (`{}`) and directly passed to the command.

```python
In [1]: constellation.Sputnik.TheFirstSatellite.initialize({})
Out[1]:
{'msg': 'transition initialize is being initiated', 'payload': None}
```

All satellites can be initialized together by sending the command to the entire constellation:

```python
In [1]: constellation.initialize({})
Out[1]:
{'Sputnik.TheFirstSatellite': {'msg': 'transition initialize is being initiated', 'payload': None},
 'Sputnik.TheSecondSatellite': {'msg': 'transition initialize is being initiated', 'payload': None}}
```

Whether the satellites have actually changed their state can be checked by retrieving the current state of an individual
satellite or the entire constellation via:

```python
In [2]: constellation.Sputnik.TheFirstSatellite.get_state()
Out[2]:
{'msg': 'init', 'payload': None}
In [3]: constellation.get_state()
Out[3]:
{'Sputnik.TheFirstSatellite': {'msg': 'init', 'payload': None},
 'Sputnik.TheSecondSatellite': {'msg': 'init', 'payload': None}}
```

Similarly, all satellite states can be called. A full list of available commands, along with a description of the finite
state machine can be found in the [concepts chapter on satellites](../concepts/satellite).

:::
::::

### Loading a Configuration File

Constellation configuration files are TOML files with the configuration key-value pairs for all satellites. The individual
satellite configurations are sent to their satellites together with the `initialize` command as dictionary.

::::{tab-set}
:::{tab-item} C++
:sync: cxx

TODO

:::
:::{tab-item} Python
:sync: python

Python provides the `tomllib` library which is used in the following to parse the TOML configuration file. The resulting
TOML data object can be directly passed to the `initialize` method, the distribution of dictionaries to the individual
satellites is taken care of by the controller.

```python
In [1]: import tomllib
In [2]: with open("test2.toml", "rb") as f:
   ...:     config = tomllib.load(f)
In [3]: constellation.satellites[0].initialize(config)
Out[3]:
{'Sputnik.TheSecondSatellite': {'msg': 'transition initialize is being initiated', 'payload': None},
 'Sputnik.TheFirstSatellite': {'msg': 'transition initialize is being initiated', 'payload': None}}
```

:::
::::


### Closing the Controller

Controllers in Constellation do not possess state and can be closed and restarted at the discretion of the user without
affecting the state of the satellites.

::::{tab-set}
:::{tab-item} C++
:sync: cxx

TODO.

:::
:::{tab-item} Python
:sync: python

The IPython CLI controller can be disconnected from the constellation using the command `exit()` or by pressing Ctrl+D.

:::
::::
