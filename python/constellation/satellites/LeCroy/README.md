---
# SPDX-FileCopyrightText: 2025 Laurent Forthomme, DESY and the Constellation authors
# SPDX-License-Identifier: CC-BY-4.0 OR EUPL-1.2
title: "LeCroy"
description: "Satellite controlling a LeCroy oscilloscope using the LeCrunch library"
---

## Description

This satellite uses the [LeCrunch3](https://github.com/nminafra/LeCrunch3) library by [Nicola Minafra](https://github.com/nminafra),
based on [LeCrunch2](https://github.com/BenLand100/LeCrunch2) and LeCrunch, to control and fetch each channel waveform from a `LeCroy Waverunner` series scope.
Using a TCP connection to the scope's `Waverunner` software, it provides a simplified method to start a single- or sequenced-waveform acquisition
and transmits it through the [CDTP](https://constellation.pages.desy.de/protocols/cdtp.html) to Constellation receivers.

## Requirements

This satellite requires the `[lecroy]` component, which can be installed with:

::::{tab-set}
:::{tab-item} PyPI
:sync: pypi

```sh
pip install ConstellationDAQ[lecroy]
```

:::
:::{tab-item} Source
:sync: source

```sh
pip install --no-build-isolation -e .[lecroy]
```

:::
::::


## Supported devices

This satellite is compatible with a broad class of `Teledyne-LeCroy` devices, and was successfully tested with a `Waverunner 8104`.
The device to be controlled by this satellite can be accessed via the `ip_address` and `port` configuration parameters.
By default the port is set to 1861 by the embedded acquisition software.

To set up the TCP transfer, run the "Utilities Setup" from the "Utilities" menu, switch to the "Remote" tab and enable the "TCPIP (VICP)" option.
The IP address to use in this satellite will be provided beside the selection box.

## Parameters

| Parameter | Description | Type | Default Value |
|-----------|-------------|------|---------------|
| `ip_address` | Scope IP address | String | - |
| `port` | TCP port to connect to | Integer | 1861 |
| `timeout` | Timeout before giving up on frames retrieval | Float | 5s |
| `nsequence` | Number of triggers to combine in a readout in sequence mode | Integer | 1 |

## Metrics

| Metric | Description | Value Type | Metric Type | Interval |
|--------|-------------|------------|-------------|----------|
| `NUM_TRIGGERS` | Number of triggers collected so far | Integer | `LAST_VALUE` | 10s |

## Custom Commands

| Command | Description | Arguments | Return Value | Allowed States |
|---------|-------------|-----------|--------------|----------------|
| `get_num_triggers` | Retrieve the number of triggers collected so far | - | Integer | any |

## Output data format

Data are packed as follows, using double precision floats for each word:

| Group | Word | Description |
|-------|------|-------------|
| Global header | Trigger times | Absolute timing of each trigger in the sequence |
|| Number of samples | Number of samples in all triggers in all sequences (run-level) |
| Channel header | Trigger offsets | Channel-level timing offset (with respect to the absolute timing in the global header) of each individual trigger in the sequence |
|| Wave array | Number of samples * number of triggers/sequence double precision floats providing the sample amplitude (in V) for the given time slice |

Additionally, a beginning-of-run event is generated with the following attributes:

| Parameter | Description | Type |
|-----------|-------------|------|
| `trigger_delay` | Absolute timing for the first trigger | String |
| `sampling_period` | Sampling period (in s) | Float |
| `channels` | Comma-separated list of channels enabled in readout | [Integers] |
| `num_sequences` | Number of triggers combined in a readout in sequence mode | Integer |
