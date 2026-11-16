---
# SPDX-FileCopyrightText: 2026 DESY and the Constellation authors
# SPDX-License-Identifier: CC-BY-4.0 OR EUPL-1.2
title: "Particulars Laser Controller"
description: "Satellite to control particulars lasers"
category: "Uncategorized"
---

## Description

This satellite controls [lasers from Particulars](https://particulars.si/products.php?prod=lasers.html), turning it on and off, as well as setting DAC and frequency. The satellite scans USB ports to find the laser automatically.

Original code written by Matias Senger, as part of his [PyticularsTCT](https://github.com/SengerM/PyticularsTCT) project. He based his code on the original C++ source code `USBM3.cxx` from Particulars. The current version of the code translates Matias's version into Constellation, with plans to implement further features from the original source code.

## Requirements

The Particulars Laser Controller satellite requires the `[particulars]` component, which can be installed with:

::::{tab-set}
:::{tab-item} PyPI
:sync: pypi

```sh
pip install "ConstellationDAQ[particulars]"
```

:::
:::{tab-item} Source
:sync: source

```sh
pip install --no-build-isolation -e ".[particulars]"
```

:::
::::

## Parameters

| Parameter   | Description           | Type  | Allowed range  |
|-------------|-----------------------|-------|----------------|
| `frequency` | Laser frequency in Hz | Float |`50.0` - `100e3`|
| `dac`       | Laser DAC value       | Int   | `0` - `1023`   |
