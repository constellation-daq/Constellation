---
# SPDX-FileCopyrightText: 2026 Andreas Pettersson, DESY and the Constellation authors
# SPDX-License-Identifier: CC-BY-4.0 OR EUPL-1.2
# Motor-controller and stage_config original by Daniil D.Rastorguev
title: "Standa"
description: "Satellite controlling a Standa motor controller using the ximc library"
category: "Test stand motor controllers"
---

## Description

This satellite uses the [libximc](https://pypi.org/project/libximc/) library to control the Standa motor controllers of a multi-axis translation stage, as used for example to position a sample in a Transient Current Technique (TCT) setup.

Each axis is driven by its own controller and is addressed by a name such as `x`, `y` or `z`. The mapping from name to controller is given by the serial number of the controller, so that an axis always keeps its name independent of the order in which the devices are enumerated.

The position of the stage is a configuration parameter and is changed with the `reconfigure` transition. This makes a position scan a sequence of runs, each taken at a fixed position, see [below](#taking-data-at-several-positions).

## Requirements

This satellite requires the `[standa]` component, which can be installed with:

::::{tab-set}
:::{tab-item} PyPI
:sync: pypi

```sh
pip install "ConstellationDAQ[standa]"
```

:::
:::{tab-item} Source
:sync: source

```sh
pip install --no-build-isolation -e ".[standa]"
```

:::
::::

## Supported devices

The stage model is selected with the `model` configuration parameter. It determines the profile of controller settings (feedback, home, motion and limit switch settings) which is applied to every controller after connecting to it. The following models are currently supported:

### `8MT30_50`

The `8MT30-50` is a motorized linear stage with 50 mm of travel and a step size of 1.25 µm. Positioning uses back-EMF feedback rather than an encoder, so the satellite cannot independently verify that a requested position was reached.

## Parameters

| Parameter | Description | Type | Default Value |
|-----------|-------------|------|---------------|
| `model` | Stage model, selects the controller profile to apply | String | - |
| `axis_serials` | Section mapping each axis name to the serial number of its controller | Section | - |
| `position` | Section with the position of each axis in mm | Section | - |
| `calibrate_on_launch` | Move all stages to their low limit switch and define zero there when launching | Bool | `false` |

The axes are named by the keys of the `axis_serials` section, and the same names have to be used in the `position` section. The `position` section has to be present even when the stage is positioned from a controller, since only parameters which exist in the configuration can be changed with {bdg-secondary}`reconfigure`.

```toml
[Standa._default]
model = "8MT30_50"
calibrate_on_launch = false

[Standa._default.axis_serials]
x = 31015
y = 31016
z = 30954

[Standa._default.position]
x = 0.0
y = 0.0
z = 0.0
```

Positions outside the travel range of the stage are rejected rather than clipped, so that data can never be recorded at a position other than the configured one.

## Metrics

| Metric | Description | Value Type | Interval |
|--------|-------------|------------|----------|
| `POS_X` | Current position of the x axis | Float | 5s |
| `POS_Y` | Current position of the y axis | Float | 5s |
| `POS_Z` | Current position of the z axis | Float | 5s |

## Custom Commands

| Command | Description | Arguments | Return Value | Allowed States |
|---------|-------------|-----------|--------------|----------------|
| `home` | Move all axes to their low limit switch and define zero there. The stage is left at zero. | - | Position of each axis (Section) | `INIT`, `ORBIT` |

## Taking data at several positions

The satellite holds its position for the duration of a run. A scan over positions is therefore performed by a controller which reconfigures the position between runs:

```python
for x in numpy.arange(0.0, 40.1, 10.0):
    for y in numpy.arange(0.0, 40.1, 10.0):
        # Move the stage and wait until all satellites are back in ORBIT
        ctrl.constellation.Standa.One.reconfigure({"position": {"x": x, "y": y}})
        ctrl.await_state(SatelliteState.ORBIT)

        # Take data at this position. Run identifiers may only contain word
        # characters and dashes, so the decimal point has to be replaced
        ctrl.constellation.start(f"x{x:.3f}_y{y:.3f}".replace(".", "p"))
        ctrl.await_state(SatelliteState.RUN)
        ...
        ctrl.constellation.stop()
        ctrl.await_state(SatelliteState.ORBIT)
```
