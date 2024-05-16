# Start a Satellite & Control It

Satellites and controllers can be implemented in either Python and in C++. Due to the shared communication protocols, a Python controller can control a C++ satellite and vice versa.

## Starting a Satellite

::::{tab-set}

:::{tab-item} C++
:sync: keyC

To start a satellite, three things need to be provided:

- `--type, -t`. Type of satellite. This corresponds to the class name of the satellite implementation.
- `--name, -n`. Name for the satellite. This name is a user-chosen name and should be unique within the constellation.
- `--group, -g`. This is the name of the Constellation group this satellite should be a part of.

Example

```sh
./build/cxx/constellation/exec/satellite -t prototype -n TheFirstSatellite -g myLabPlanet
```

:::

:::{tab-item} Python
:sync: keyP

The virtual environment that had been created when installing the framework needs to be activated where necessary

`source venv/bin/activate`

When a satellite is started, there is the option to provide a number of parameters, for example:

- `--name, -n`. Name for the satellite. This name is a user-chosen name and should be unique within the constellation.
- `--group, -g`. This is the name of the Constellation group this satellite should be a part of.

```sh
python -m constellation.core.controller --name TheFirstSatelline --group myLabPlanet
```

:::

::::

## Controlling a Satellite

### Starting a Controller

::::{tab-set}

:::{tab-item} C++
:sync: keyC

To control the satellite created in the first part of this tutorial, the controller needs to be in the same group.

TODO

:::

:::{tab-item} Python
:sync: keyP

One option to control a satellite is the Constellation CLI IPython Controller.

Upon starting, it is possible to pass it some (optional) arguments, for example:

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
