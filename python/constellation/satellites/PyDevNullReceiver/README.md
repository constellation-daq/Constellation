---
# SPDX-FileCopyrightText: 2025 DESY and the Constellation authors
# SPDX-License-Identifier: CC-BY-4.0 OR EUPL-1.2
title: "PyDevNullReceiver"
description: "A Python satellite that receives data and drops it"
category: "Developer Tools"
---

## Custom Commands

| Command | Description | Arguments | Return Value | Allowed States |
|---------|-------------|-----------|--------------|----------------|
| `get_data_rate` | Get data rate during the last run in Gbps | - | data rate, `double` | `ORBIT` |
