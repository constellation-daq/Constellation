---
# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC-BY-4.0 OR EUPL-1.2
title: "Sputnik"
description: "Demonstrator satellite serving as prototype for new satellites"
category: "Example Templates"
---

## Description

This satellite does very little apart from beeping, just as its [namesake](https://en.wikipedia.org/wiki/Sputnik_1). It mostly serves as demonstrator for the different functionalities of satellites. New satellites may be created by copying and modifying the Sputnik satellite.

This section describes the functions of the satellite and all relevant information about the connected hardware as well as its requirements or external software dependencies.

## Building

The Sputnik satellite has no additional dependencies and is build by default.

## Parameters

The following parameters are read and interpreted by this satellite. Parameters without a default value are required.

| Parameter  | Description | Type | Default Value |
|------------|-------------|------|---------------|
| `interval` | Interval in which beep signals are emitted in units of milliseconds | Unsigned integer | `3000` |
| `launch_delay` | Delay for launch in seconds | Unsigned integer | `0` |

This satellite supports in-orbit reconfiguration of the `interval` parameter.

### Configuration Example

An example configuration for this satellite which could be dropped into a Constellation configuration as a starting point

```toml
[Sputnik.One]
my_param = 7
other_param = "antenna"
```

## Metrics

The following metrics are distributed by this satellite and can be subscribed to. Timed metrics provide an interval in units of time, triggered metrics in number of calls.

| Metric | Description | Value Type | Metric Type | Interval |
|--------|-------------|------------|-------------|----------|
| `BEEP` | Sputnik beep signal | Integer | `LAST_VALUE` | configurable, default 3s |
| `TEMPERATURE` | Temperature inside the spacecraft in degrees Celsius | Float | `LAST_VALUE` | 3s |
| `FAN_RUNNING` | Boolean indicating if the internal fan is running | Bool | `LAST_VALUE` | 5s |
| `TIME` | Time since launch in seconds | Float | `LAST_VALUE` | 10s |

## Custom Commands

This section describes all custom commands the satellite exposes to the command interface. The description should contain the name and the description of the
command as well as all of its arguments, the return value and the allowed states:

| Command | Description | Arguments | Return Value | Allowed States |
|---------|-------------|-----------|--------------|----------------|
| `get_channel_reading` | This command returns the reading from the given channel number | channel number, `int` | channel reading, `double` | `NEW`, `INIT`, `ORBIT` |
