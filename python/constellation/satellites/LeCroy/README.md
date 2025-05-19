---
# SPDX-FileCopyrightText: 2025 Laurent Forthomme, DESY and the Constellation authors
# SPDX-License-Identifier: CC-BY-4.0 OR EUPL-1.2
title: "LeCroy"
description: "Satellite controlling a LeCroy oscilloscope using the LeCrunch library"
---

## Description

This satellite uses the [LeCrunch3](https://github.com/nminafra/LeCrunch3) library by [Nicola Minafra](https://github.com/nminafra),
based on [LeCrunch2](https://github.com/BenLand100/LeCrunch2) and LeCrunch, to control and fetch the waveforms from a LeCroy Waverunner scope.
Using a TCP connection to the scope's Waverunner software, it provides a simplified method to start a single- or sequenced-waveforms acquisition
and transmits it _via_ the [CDTP](https://constellation.pages.desy.de/protocols/cdtp.html) to Constellation receivers.

## Requirements

The LeCroy satellite requires the `[LeCrunch3]` Python module, which can either be installed [from sources](https://github.com/nminafra/LeCrunch3)
or using [the version packaged on Pypi](https://pypi.org/project/LeCrunch3/):

::::{tab-set}
:::{tab-item} PyPI
:sync: pypi

```sh
pip install LeCrunch3
```


## Supported devices

The device to be controlled by this satellite can be accessed via the `ip_address` and `port` configuration parameters.

## Parameters

| Parameter | Description | Type | Default Value |
|-----------|-------------|------|---------------|
| `ip_address` | Scope IP address | String | - |
| `port` | TCP port to connect to | Integer | 1861 |
| `timeout` | Timeout before giving up on frames retrieval | Float | 5s |
| `nsequence` | Number of triggers to combine in a readout in sequence mode | Integer | 1 |

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
