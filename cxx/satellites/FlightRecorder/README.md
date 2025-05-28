---
# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC-BY-4.0 OR EUPL-1.2
title: "Flight Recorder"
subtitle: "Satellite to record log messages from a Constellation"
---

## Description

This satellite

- configure global log level to listen to
- configure per-satellite log level or topic to listen to
- provide file pattern to be used
- handover to new log file happens with `starting`

 FILE,   // Simple log file
        ROTATE, // Multiple log files, rotate logging by file size
        DAILY,  // Create a new log file daily at provided time
        RUN,    // Create a new log file whenever a new run is started

flushing after period or when run stopped or interrupted

## Parameters

The following parameters are read and interpreted by this satellite. Parameters without a default value are required.

| Parameter | Type | Description | Default Value |
|-----------|------|-------------|---------------|
| `method` | String | Method to be used for logging. Valid values are `FILE`, `ROTATE`, `DAILY` and `RUN` | `FILE` |
| `file_path` | String | Path to the target log file | - |
| `allow_overwriting` | Boolean | Flag to allow or deny overwriting of existing log files | `false` |
| `global_recording_level` | String | Global log level to be recorded by this satellite | `WARNING` |
| `flush_period` | Integer | Period in seconds after which log messages are regularly flushed to storage | `10` |
| `rotate_max_files` | Integer | Maximum number of files to be user for rotating. Only used for `method = "ROTATE"` | `10` |
| `rotate_filesize` | Integer | Maximum file size Mb after which the log is rotated. Only used for `method = "ROTATE"` | `100` |
| `daily_switching_time` | Time point | Point in time at which the log file should be switched. Only used for `method = "DAILY"` | - |

### Configuration Example

An example configuration for this satellite which could be dropped into a Constellation configuration as a starting point

```ini
[FlightRecorder.RunLogger]
method = "RUN"
allow_overwriting = true
file_path = "/data/logs/logfile.txt"
```

## Metrics

The following metrics are distributed by this satellite and can be subscribed to.

| Metric | Description | Interval | Type |
|--------|-------------|----------|------|
| `MSG_TOTAL` | Total number messages received and logged since satellite startup | 3s | `LAST_VALUE` |
| `MSG_WARN` | Number of warning messages received and logged since satellite startup | 3s | `LAST_VALUE` |
| `MSG_RUN` | Total number messages received and logged since the last run start | 3s | `LAST_VALUE` |
