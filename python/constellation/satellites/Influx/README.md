---
# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC-BY-4.0 OR EUPL-1.2
title: "Influx"
description: "Satellite writing metrics to InfluxDB"
category: "Monitoring"
---

## Description

This satellite listens to metrics sent by other satellites and writes to a InfluxDB time series database.
Only metrics of type float, integer and boolean can be written to InfluxDB. Writing to the database is performed in batched
mode, this means received telemetry data are buffered and flushed to InfluxDB after a pre-defined time set via the
`flush_interval`. When increasing this interval it should be noted that newly received telemetry data will only show up in
any graphical display after it has been flushed to the database and might not be available yet when the display is updated.

```{warning}
Currently, the satellites subscribes to all metrics. This can lead to performance penalties if there are debugging metrics
which are evaluated in a hot loop. Excluding and including metrics is a feature that will be added in the future.
```

## Requirements

The Influx satellite requires the `[influx]` component, which can be installed with:

::::{tab-set}
:::{tab-item} PyPI
:sync: pypi

```sh
pip install "ConstellationDAQ[influx]"
```

:::
:::{tab-item} Source
:sync: source

```sh
pip install --no-build-isolation -e ".[influx]"
```

:::
::::

```{note}
The Influx satellite requires a working InfluxDB instance. A how-to guide on how to set up an InfluxDB instance is given in
the [operator guide](../../operator_guide/howtos/setup_influxdb_grafana).
```

## Parameters

| Parameter | Description | Type | Default Value |
|-----------|-------------|------|---------------|
| `url` | InfluxDB URL | String | `http://localhost:8086` |
| `token` | Access token | String | - |
| `org` | Organization | String | - |
| `bucket` | Measurement bucket | String | `constellation` |
| `flush_interval` | Interval in seconds with which telemetry data is flushed to the InfluxDB | Float | 2.5 |
