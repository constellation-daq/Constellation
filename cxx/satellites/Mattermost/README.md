---
# SPDX-FileCopyrightText: 2025 DESY and the Constellation authors
# SPDX-License-Identifier: CC-BY-4.0 OR EUPL-1.2
title: "Mattermost"
description: "Satellite sending log messages to Mattermost"
category: "Monitoring"
---

## Description

This satellite listens to log messages sent by other satellites and sends them to a Mattermost channel. Additionally, log
messages are sent when a run is started, stopped or interrupted. Log messages with a log level of `WARNING` or `CRITICAL` are
prefixed with `@channel` to notify all users in the channel.

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
