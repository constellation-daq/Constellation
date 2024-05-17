# Starting & Controlling a Satellite

Satellites and controllers can be implemented in either Python and in C++. Due to the shared communication protocols, a
Python controller can control a C++ satellite and vice versa. The following tutorial describes how to start a single
satellite and how to discover it and change its state with a controller.

## Starting a Satellite

::::{tab-set}

:::{tab-item} C++
:sync: keyC

The `satellite` executable starts a new Constellation satellite and requires three command line arguments:

- `--type, -t`: Type of satellite. This corresponds to the class name of the satellite implementation.
- `--name, -n`: Name for the satellite. This name is a user-chosen name and should be unique within the constellation.
- `--group, -g`: This is the name of the Constellation group this satellite should be a part of.

A call with all three parameters provided could e.g. look like follows:

```sh
./build/cxx/constellation/exec/satellite -t Prototype -n TheFirstSatellite -g MyLabPlanet
```

:::

:::{tab-item} Python
:sync: keyP

Before starting a Python satellite, the virtual environment created during the installation of the framework might need to be
reactivated using `source venv/bin/activate`.

The satellite is then directly started via its Python module. Additional parameters can be supplied, such as:

- `--name, -n`. Name for the satellite. This name is a user-chosen name and should be unique within the constellation.
- `--group, -g`. This is the name of the Constellation group this satellite should be a part of.

Starting the example satellite implementation provided with the framework would therefore look like follows:

```sh
python -m constellation.satellites.example_satellite --name TheFirstSatellite --group myLabPlanet
```

:::

::::

## Controlling the Satellite

A controller running in the same Constellation group is needed in order to control the satellite started in the first part
of this tutorial. This section shows different options to perform this task.

### Starting a Controller

::::{tab-set}

:::{tab-item} C++
:sync: keyC

TODO

:::

:::{tab-item} Python
:sync: keyP

The Python implementation of Constellation provides a powerful command line interface controller using IPython. This package
needs to be installed (`pip install ipython`) in the virtual environment that was created during the installation of the
framework and can be reactivated using `source venv/bin/activate`.

The controller is started via its Python module and requires Upon starting, it is possible to pass it some (optional) arguments, for example:

- `--name`. A name for the controller (default: cli_controller)
- `--group`. The constellation group to which the controller should belong (default: constellation)
- `--log-level`. The logging verbosity for the controller's log messages (default: info)

To control the satellite created in the first part of this tutorial, the controller needs to be in the same group.

```sh
python -m constellation.core.controller --group myLabPlanet
```

To following command can be used to see how many satellites are connected:

```python
constellation.satellites
```

Example output

```sh
[<__main__.SatelliteCommLink at 0x78d5bba4fcd0>]
```

To obtain more information on a satellite, it can be addressed directly.

```python
print(constellation.satellites[0].get_name())
```

Example output

```sh
{'msg': 'prototype.thefirstsatellite', 'payload': None}
```

:::

::::

### Sending Commands to the Satellite

::::{tab-set}

:::{tab-item} C++
:sync: keyC

TODO

:::

:::{tab-item} Python
:sync: keyP

Commands can be sent to single satellites, all satellites of one type, or the entire constellation.

The procedure to see a list of available commands is to start typing and then to hit the tab key.

To initialize the satellite, it needs to be sent an initialize command, with a dictionary of config options as an argument.

```python
constellation.satellites[0].initialize({})
```

Example output

```sh
{'msg': 'transition initialize is being initiated', 'payload': None}
```

The following command can be used to check on the state of the satellite

```python
constellation.satellites[0].get_state()
```

Example output

```sh
{'msg': 'init', 'payload': None}
```

TODO: nice wrap-up sentence that doesn't sound super odd in passive voice.

:::

::::

### Terminating the Controller

::::{tab-set}

:::{tab-item} C++
:sync: keyC

TODO.

:::

:::{tab-item} Python
:sync: keyP

The controller can be disconnected from the constellation using the command `exit`.

:::

::::
