# Start a Satellite & Control It

Satellites and controllers can be implemented in either Python and in C++. Due to the shared communication protocols, a Python controller can control a C++ satellite and vice versa.

## Starting a Satellite

::::{tab-set}

:::{tab-item} C++
:sync: keyC

To start a satellite, you need to provide three things:

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

Remember to activate the virtual environment you created when installing the framework where necessary

`source venv/bin/activate`

When you start a satellite, you have the option to provide a number of parameters, for example:

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

To see how many satellites are connected, type

```python
constellation.satellites
```

Example output

```sh
[<__main__.SatelliteCommLink at 0x78d5bba4fcd0>]
```

To obtain more information on a satellite, you can address it directly.

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

This will follow at a later date. And then you will know how to change the state of your satellite!

:::

:::{tab-item} Python
:sync: keyP

Commands can be sent to single satellites, all satellites of one type, or the entire constellation.

To see a list of available commands, start typing and then hit the tab key.

To initialize the satellite, you need to send it an initialize command, with a dictionary of config options as an argument.

```python
constellation.satellites[0].initialize({})
```

Example output

```sh
{'msg': 'transition initialize is being initiated', 'payload': None}
```

To check on the status of the satellite after sending a command

```python
constellation.satellites[0].get_state()
```

Example output

```sh
{'msg': 'init', 'payload': None}
```

Now you know how to change the state of your satellite!

:::

::::

### Terminating the Controller

::::{tab-set}

:::{tab-item} C++
:sync: keyC

Ctrl-C FTW ;-)

Just kidding, tba.

:::

:::{tab-item} Python
:sync: keyP

Type `exit` to disconnect the controller from the constellation.

:::

::::
