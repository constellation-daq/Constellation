---
# SPDX-FileCopyrightText: 2025 DESY and the Constellation authors
# SPDX-License-Identifier: CC-BY-4.0 OR EUPL-1.2
title: "Mattermost"
description: "Satellite sending log messages to Mattermost"
category: "Monitoring"
---

## Description

This satellite listens to log messages sent by other satellites and sends them to a Mattermost channel. The name of the
sender will be used as username in Mattermost. Log messages with a log level of `WARNING` are marked as `important` and log
messages with a log level of `CRITICAL` are marked as urgent. In both cases the log messages are prefixed with `@channel` to
notify all users in the channel. In addition to logging from other satellites, messages are sent when a run is started,
stopped or interrupted.

When sending the message to Mattermost fails temporarily, the sending is retried a configurable number of times with a
quadratically increasing back-off time between the trials.

## Building

The Mattermost requires [`cpr`](https://github.com/libcpr/cpr), which is downloaded on demand.
The satellite is not build by default, building can be enabled via:

```sh
meson configure build -Dsatellite_mattermost=true
```

## Parameters

| Parameter  | Description | Type | Default Value |
|------------|-------------|------|---------------|
| `webhook_url` | URL of the Mattermost webhook | string | - |
| `log_level` | Minimum log level of the logger | string | `WARNING` |
| `subscribe_topics` | Section with individual log topics and the respective subscription log level. | Section | {`OP`: `INFO`} |
| `ignore_topics` | Ignore log messages with certain topics | list of strings | [`FSM`] |
| `only_in_run` | Only log to Mattermost in the `RUN`, `interrupting` or `SAFE` state | bool | `false` |
| `max_retries` | Number of retries for sending a message to Mattermost | Integer | 5 |
| `backoff_time` | Initial time in milliseconds for the back-off between retrying to send messages | Integer | 500 |

The `subscribe_topics` section can be used to change the subscription levels of individual topics beyond the global `log_level` setting.
For example the `OP` topic logging operator actions will be subscribed to on `INFO` log level by default. Other topics can be
added similarly:

```yaml
Mattermost:
  Logger:
    subscribe_topics:
      OP: "INFO"
      CTRL: "DEBUG"
```

The parameters `ignore_topics` and `subscribe_topics` are mutually exclusive, and a log topic can only appear in one of them.
