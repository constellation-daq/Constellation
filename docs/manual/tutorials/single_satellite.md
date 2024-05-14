# Start a Satellite & Control It

## Starting a Satellite

To start a satellite, you need to provide three things:

- Type of satellite. This corresponds to the class name of the satellite implementation
- Group. The name of the constellation group this satellite should be a part of
- Name for the satellite. This name is a user-chosen name and should be unique within the group.

Example

::::{tab-set}

:::{tab-item} C++
:sync: keyC

```sh
./build/cxx/constellation/exec/satellite -t prototype -g myLabPlanet -n TheFirstSatellite
```

:::

:::{tab-item} Python
:sync: keyP

tba
:::

::::

::::{tab-set}

:::{tab-item} C++
:sync: keyC

tba
:::

:::{tab-item} Python
:sync: keyP

```sh
python -m constellation.core.controller --group myLabPlanet
```

Arguments for the python controller are

- name -  name of controller
- group -  group of controller
- interface - the interface to connect to

:::

::::
