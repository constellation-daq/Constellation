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
| `ignore_topics` | Ignore log messages with certain topics | list of strings | [`FSM`] |
| `only_in_run` | Only log to Mattermost in the `RUN`, `interrupting` or `SAFE` state | bool | `false` |
