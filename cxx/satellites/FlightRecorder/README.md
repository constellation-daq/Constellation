---
# SPDX-FileCopyrightText: 2025 DESY and the Constellation authors
# SPDX-License-Identifier: CC-BY-4.0 OR EUPL-1.2
title: "FlightRecorder"
description: "Satellite to record log messages from a Constellation"
category: "Monitoring"
---

## Description

This satellite subscribes to Constellation log messages of a configurable level, receives them and records them to storage.
Several storage options are available and can be selected via the `method` configuration parameter:

* `FILE` represents the simplest storage, all logs are recorded into a single file provided via the `file_path` parameter.
* `ROTATE` allows to use multiple, rotating log files. Every time the current log file reaches the size configured via the
  `rotate_filesize` parameter, a new file is started. After the maximum number of files set by `rotate_max_files` is reached,
  the oldest log file is overwritten. This can be useful to keep only a certain amount of history present.
* `DAILY` will create a new log file every 24h at the time defined with the `daily_switch` parameter. This method is
  particularly useful for very long-running Constellations which require preservation of the log history.
* `RUN` will switch to a new log file whenever a new run is started, i.e. when this satellite received the `start` command.
  This can be especially helpful when many runs are recorded and an easy assignment of logs is required.

The global log level to which the FlightRecorder subscribes for all satellites in the Constellation can be configured using
the `global_recording_level`.

```{caution}
This level should be selected carefully. Only log messages with an active subscription are transmitted over the network and
subscribing to low levels such as `DEBUG`  or `TRACE` might lead to significant network traffic and sizable log files being
written to storage.
```

Log messages are cached and flushed to storage asynchronously. The cache is flushed in regular intervals set by
`flush_period` as well as at each run stop or interruption of operation.

```{note}
It should be noted that the `flush_period` setting also applied to the console output of the FlightRecorder satellite itself.
```

## Parameters

The following parameters are read and interpreted by this satellite. Parameters without a default value are required.

| Parameter | Type | Description | Default Value |
|-----------|------|-------------|---------------|
| `method` | String | Method to be used for logging. Valid values are `FILE`, `ROTATE`, `DAILY` and `RUN` | - |
| `file_path` | String | Path to the target log file | - |
| `allow_overwriting` | Boolean | Flag to allow or deny overwriting of existing log files | `false` |
| `global_recording_level` | String | Global log level to be recorded by this satellite | `WARNING` |
| `flush_period` | Integer | Period in seconds after which log messages are regularly flushed to storage | `10` |
| `rotate_max_files` | Integer | Maximum number of files to be user for rotating. Only used for `method = "ROTATE"` | `10` |
| `rotate_filesize` | Integer | Maximum file size Mb after which the log is rotated. Only used for `method = "ROTATE"` | `100` |
| `daily_switching_time` | Local time | Local time in the format `hh:mm:ss` at which the log file should be switched. Only used for `method = "DAILY"` | - |

### Configuration Example

An example configuration for this satellite which could be dropped into a Constellation configuration as a starting point

```toml
[FlightRecorder.RunLogger]
method = "RUN"
allow_overwriting = true
file_path = "/data/logs/logfile.txt"
```

## Metrics

| Metric | Description | Interval | Type |
|--------|-------------|----------|------|
| `MSG_TOTAL` | Total number messages received and logged since satellite startup | 3s | `LAST_VALUE` |
| `MSG_WARN` | Number of warning messages received and logged since satellite startup | 3s | `LAST_VALUE` |
| `MSG_RUN` | Total number messages received and logged since the last run start | 3s | `LAST_VALUE` |

## Custom Commands

| Command | Description | Arguments | Return Value | Allowed States |
|---------|-------------|-----------|--------------|----------------|
| `flush` | Flush log sink | - | - | `INIT`, `ORBIT`, `RUN`, `SAFE` |
