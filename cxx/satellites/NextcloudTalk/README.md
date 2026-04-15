---
# SPDX-FileCopyrightText: 2026 DESY and the Constellation authors
# SPDX-License-Identifier: CC-BY-4.0 OR EUPL-1.2
title: "NextcloudTalk"
description: "Satellite sending log messages to Nextcloud Talk"
category: "Monitoring"
---

## Description

This satellite listens to log messages sent by other satellites and sends them to a Nextcloud Talk chat. Log messages with a
log level of `WARNING` and `CRITICAL` are prefixed with `@all` to notify all users in the chat. In addition to logging from
other satellites, messages are sent when a run is started, stopped or interrupted.

When sending the message to Nextcloud Talk fails temporarily, the sending is retried a configurable number of times with a
quadratically increasing back-off time between the trials.

## Building

This satellite requires [`cpr`](https://github.com/libcpr/cpr), which is downloaded on demand.
The satellite is not build by default, building can be enabled via:

```sh
meson configure build -Dsatellite_nextcloud_talk=true
```

## Parameters

| Parameter  | Description | Type | Default Value |
|------------|-------------|------|---------------|
| `host` | URL of the Nextcloud instance | string | - |
| `chat_id` | ID of the target chat, for example `n4cwk7bk`. This can be found as last part of the URL when opening the chat in the browser | string | - |
| `account` | Account name of the user to connect with | string | - |
| `app_password` | App password of the account to be used, for example `SLyPa-oTfTc-57zTn-QlPsZ-91PW7` | string | - |
| `log_level` | Minimum log level of the logger | string | `WARNING` |
| `ignore_topics` | Ignore log messages with certain topics | list of strings | [`FSM`] |
| `only_in_run` | Only log to Nextcloud Talk in the `RUN`, `interrupting` or `SAFE` state | bool | `false` |
| `max_retries` | Number of retries for sending a message to Nextcloud Talk | Integer | 5 |
| `backoff_time` | Initial time in milliseconds for the back-off between retrying to send messages | Integer | 500 |
