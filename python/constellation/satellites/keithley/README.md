---
# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC-BY-4.0 OR EUPL-1.2
title: "Keithley"
subtitle: "Satellite controlling a Keithley power supply"
---

## Requirements

The Keithley satellite requires the `[keithley]` component, which can be installed e.g. via:

```sh
pip install --no-build-isolation -e .[keithley]
```

## Description

Keithleys have an RS-232 serial port, which can be used to control them remotely. This satellite controls a single Keithley
as voltage source. It allows to set the OVP, compliance and target voltage to which it always ramps in small voltage steps.

```{note}
Access to serial ports usually requires root access. This can be avoid by giving the current user the appropriate rights.
On Unix, if the port is e.g. `/dev/ttyUSB0`, the group can be found via `stat /dev/ttyUSB0` (the Gid entry).
If the group is e.g. `dialout`, the permissions can be given with `sudo usermod -a -G dialout $USER`.
A restart is required for the changes to be effective.
```

## Supported devices

### `2410`

The `2410` device supports the Kethley Series 2400 SourceMeter in RS-232 mode.
The following communication settings are required:

- `BAUD`: `19200`
- `BITS`: `8`
- `PARITY`: `EVEN`
- `TERMINATOR`: `<CR+LF>`
- `FLOW-CTRL`: `NONE`

## Parameters

| Parameter | Description | Type | Default Value |
|-----------|-------------|------|---------------|
| `device` | Device | String | - |
| `port` | Serial port to connect to | String | - |
| `voltage` | Target output voltage | Floating Point Number | - |
| `voltage_step` | Voltage step in which to ramp to target voltage | Floating Point Number | - |
| `settle_time` | Time to wait before continuing with the next voltage step | Floating Point Number | - |
| `ovp` | Voltage limit for over-voltage protection | Floating Point Number | - |
| `compliance` | Current limit in Ampere | Floating Point Number | - |

## Custom Commands

| Command | Description | Arguments | Return Value | Allowed States |
|---------|-------------|-----------|--------------|----------------|
| `read_output` | Reads voltage and current output | - | - | `ORBIT`, `RUN` |
| `in_compliance` | Check if the current is in compliance | - | - | `ORBIT`, `RUN` |
