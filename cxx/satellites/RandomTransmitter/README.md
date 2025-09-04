---
# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC-BY-4.0 OR EUPL-1.2
title: "RandomTransmitter"
description: "A satellite that transmits random data"
category: "Developer Tools"
---

## Description

This satellite creates random data and sends it out as fast as possible, allowing to test the network performance.
Random data can either be generated continuously during the run or once at the beginning of the run, which is faster.

When data sending is limited because the framework or the receiver cannot handle the data rate, the transmitter will sleep
for 1ms. The duty cycle resulting from this is tracked in the `DUTY_CYCLE` metric.

## Building

The RandomTransmitter satellite has no additional dependencies.
The satellite is not build by default, building can be enabled via:

```sh
meson configure build -Dsatellite_random_transmitter=true
```

## Parameters

| Parameter | Type | Description | Default Value |
|-----------|------|-------------|---------------|
| `pregen` | Bool | Use pre-generated data | `false` |
| `seed` | Unsigned 32-bit integer | Seed for the random engine | Random |
| `block_size` | Unsigned integer | Size of a data block in bytes | `1024` |
| `number_of_blocks` | Unsigned integer | Number of data blocks per data message | `1` |

## Metrics

| Metric | Description | Value Type | Metric Type | Interval |
|--------|-------------|------------|-------------|----------|
| `DUTY_CYCLE` | Total duty cycle of the run loop | Integer | `LAST_VALUE` | 5s |
