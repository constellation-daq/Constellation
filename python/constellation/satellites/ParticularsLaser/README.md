---
# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC-BY-4.0 OR EUPL-1.2
title: "Particulars Laser Controller"
description: "Satellite to control particulars lasers"
category: "Uncategorized"
---

## Description

This satellite controls lasers from Particulars, turnig it on and off, as well as setting DAC and frequency. Code has been adapted from (link) by Mattias Senger.

## Parameters

| Parameter | Description | Type | Default Value |
|-----------|-------------|------|---------------|
| `frequency` | Laser frequency in Hz | Float | `5.0` |
| `DAC` | Laser DAC value | Int | `0.1` |
