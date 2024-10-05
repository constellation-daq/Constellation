---
# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC-BY-4.0 OR EUPL-1.2
title: "eCT Stage"
subtitle: "Satellite controlling the ThorLab LT300C linear and PRM1/MZ8 rotation stages"
---

## Description

This satellite allows the control of the ThorLab LT300C linear and PRM1/MZ8 rotation stages via the `pylablib` python package

## Prerequisites
Python package `pylablib` is required to steer the linear and rotational stages. Install using command:
```
pip install pylablib
```

## Initialization Parameters

To initialize `SatelliteECTstage`, one parameter is required.

| Parameter       | Description                                                                                                        | Type      |
|-----------------|--------------------------------------------------------------------------------------------------------------------|-----------|
| `config_file`   | Configuration file (`.toml`) containing all required variable. See Configuration File section for more information | string    |                  


## Configuration File

a `.toml` configuration file is needed to configure the satellite. The following parameters need to be specified in the configuration file. System and connection parameters are required.

### Stage Parameters
Declare as `[{stage_axis}]` eg: `[x]`,`[y]`,`[z]`,`[r]`. All parameters must be defined in config file.

| Parameter       | Description                                                            | Type      | Default Value [+] | Safety Limit                   |
|-----------------|------------------------------------------------------------------------|-----------|-------------------|--------------------------------|
| `port`          | Serial port name (eg:`"/dev/ttyUSB0"`)                                 | string    | -                 | -                              |
| `chan`          | Channel number if multiple stages are moved via same serial connection | number    | `0`               | -                              |
| `velocity`      | Velocity of the stage movement in `mm/s`                               | int/float | `1`               | max=`20`                       |
| `acceleration`  | Acceleration of the stage movement in `mm/s^2`                         | int/float | `1`               | max=`10`                       |
| `start_position`| Start Position of all new runs in `mm`. See "Modes of Operations"      | int/float | -                 | `0` to `290` for linear stages |

[+]Use the given default value if unsure of value

### Run Parameters
Declare as `[run]`. All parameters must be defined in config file.

| Parameter               | Description                                  | Type                                   | Safety Limit                   |
|-------------------------|----------------------------------------------|----------------------------------------|--------------------------------|
| `active_axes`           | Axes/stages that must be initialised         | list of axis names eg: `["x","y"]`     | -                              |
| `pos_{stage_axis}`      | in `mm`. See "Modes of Operations"           | three-vector list  (int/float)         | `0` to `290` for linear stages |
| `stop_time_per_point_s` | wait time at each point (measurement window) in `s` | float                           | -                              |
| `readout_freq_s`        | stage position sending frequency (in `s`) to data writer | float                      | -                              |
| `save_config`           | (optional) If True, the configuration file will be saved in `path/to/constellation/data` folder    | bool| -           |


### Modes of Operation

* If `pos_{stage_axis}` is undefined, the stage will move to the home position of `{stage_axis}` and take data.

* If `pos_{stage_axis}` is a three-vector eg: `[val_1,val_2,val_3]` the stage will move between `val_1` and `val_2` in steps of `val_3`.


A minimal configuration would be:

```ini
[x]
port = "/dev/ttyUSB0"
chan = 0
# in mm
velocity = 2
acceleration = 10
start_position = 10

[y]
port = "/dev/ttyUSB1"
chan = 0
# in mm
velocity = 2
acceleration = 10
start_position = 10

[z]
port = "/dev/ttyUSB3"
chan = 0
# in mm
velocity = 2
acceleration = 10
start_position = 10

[r]
port = "/dev/ttyUSB3"
chan = 0
# in deg
velocity = 8
acceleration = 8
start_position = 180

[run]
active_axes = ["x","y","z","r"]
stop_time_per_point_s = 3
readout_freq_s = 1e-3

# in mm
pos_x = [10,30,10]
pos_y = [10,30,10]
pos_z = [10,20,10]
# in deg
pos_r = [175,185,10]

save_config = true
```

## Usage
To start the Satellite, run

``` shell
SatelliteECTstage --group {group_name} --name {instance_name}
```

or

``` shell
SatelliteECTstage --help
```


## Additional Satellite Functions

* `blink(axis)`
  * Blink test stages.
  * args: `axis`. Mandatory argument


* `disable_axis(axis)`
  * Disable axis. Once disabled, the stage cannot be moved until re-enabled
  * args: `axis`. Mandatory Argument


* `disconnect(axis=None (optional))`
  * Disconnects the communication via the serial port to the stages.
  * args: `axis` (optional). if None: applies to all stages
  * Can only be executed if in INIT Satellite State


* `enable_axis(axis)`
  * Enable axis
  * args: `axis`. Mandatory Argument


* `get_full_status(axis=None (optional))`
  * Returns stage full status
  * args: `axis` (optional). if None: applies to all stages


* `get_full_info(axis=None (optional))`
  * Returns stage information including status, serial port communication information
  * args: `axis` (optional). if None: applies to all stages


*  `get_position(axis=None (optional))`
  * Get stage position
  * args: `axis` (optional). if None: applies to all stages


* `get_status(axis=None (optional))`
  * Returns stage status as string
  * args: `axis` (optional). if None: applies to all stages


* `get_vel_acc_params(axis=None (optional))`
  * Get stage max, min velocities and acceleration
  * args: `axis` (optional). if None: applies to all stages


* `go_home(axis=None (optional))`
  * Goes back to the stage defined home position. Ideally should be 0 mm for linear stages and 0 deg for rotational stage.
  * args: `axis` (optional). if None: applies to all stages


* `go_start_position(axis=None (optional))`
  * move to start position
  * args: `axis` (optional). if None: applies to all stages


* `stage_stop(axis=None (optional))`
  * Stops stages. Only works outside of main loop.
    (NOTE: For emergency stop while within run loop, use stop())
  * args: `axis` (optional). if None: applies to all stages