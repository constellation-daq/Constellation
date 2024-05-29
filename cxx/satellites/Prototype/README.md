---
# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC-BY-4.0 OR EUPL-1.2
title: "Prototype"
description: "Prototype satellite demonstrating how to implement individual satellites"
services: ["CONTROL", "MONITORING", "DATA", "HEARTBEAT"]
---

## Description

This section will describe the functionality of the satellite and any relevant information about attached hardware and requirements thereof.

## Parameters

* `my_param`: Description of this parameter with its default value and the information whether it is required or not.

## Configuration Example

An example configuration for this satellite which could be dropped into a COnstellation configuration as a starting point

```ini
[prototype.my_satellite]
my_param = 7
```

## Custom Commands

This section describes all custom commands the satellite exposes to the command interface. The description should contain the name and the description of the
command as well as all of its arguments, the return value and the allowed states:

* `get_channel_reading`: This command returns the reading from the given channel number
  * Arguments: channel number (`int`)
  * Return value: channel reading (`double`)
  * Allowed states: `INIT`, `ORBIT`
