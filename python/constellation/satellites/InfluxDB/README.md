---
# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC-BY-4.0 OR EUPL-1.2
title: "InfluxDB"
subtitle: "Satellite writing metrics to InfluxDB"
---

## Description

This satellite listens to all metrics send by other satellites and writes to a InfluxDB time series database.
Only metrics of type float, integer and boolean can be written to InfluxDB.

## Requirements

The Keithley satellite requires the `[influxdb]` component, which can be installed e.g. via:

```sh
pip install --no-build-isolation -e .[influxdb]
```

## Parameters

| Parameter | Description | Type | Default Value |
|-----------|-------------|------|---------------|
| `url` | InfluxDB URL | String | `http://localhost:8086` |
| `token` | Access token | String | - |
| `org` | Organization | String | - |
| `bucket` | Measurement bucket | String | `constellation` |
