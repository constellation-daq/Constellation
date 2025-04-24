---
# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC-BY-4.0 OR EUPL-1.2
title: "H5DataWriter"
description: "Satellite receiving data and writing it to HDF5 files"
category: "Data Receivers"
---

## Description

This satellite receives data from all satellites and stores it in an HDF5 file.

## Requirements

The H5DataWriter satellite requires the `[hdf5]` component, which can be installed with:

::::{tab-set}
:::{tab-item} Source
:sync: source

```sh
pip install --no-build-isolation -e .[hdf5]
```

:::
:::{tab-item} PyPI
:sync: pypi

```sh
pip install ConstellationDAQ[hdf5]
```

:::
::::

This requires the HDF5 development libraries to be installed, which can be installed with:

::::{tab-set}
:::{tab-item} Debian/Ubuntu

```sh
sudo apt install libhdf5-dev
```

:::
:::{tab-item} ALMA/Fedora

TODO

:::
:::{tab-item} MacOS

TODO

:::
:::{tab-item} Windows

TODO

:::
::::

## Configuration

### Flush interval

The configuration variable `flush_interval` is a floating point value that controls after how many seconds the HDF5 file is being flushed, i.e. being written to disk.

The default value is `10.0` seconds. Smaller values will ensure that the file is being updated more frequently but might impact performance.

### Allow Concurrent Reading (aka 'Single-Writer-Multiple-Reader' mode)

Usually, it is not safe to read from an HDF5 file while data is being written to it. However, a special writing mode available in recent versions of HDF5 allows such concurrent reading:  ["single-writer multiple-reader" (SWMR) mode](https://support.hdfgroup.org/documentation/hdf5/latest/_s_w_m_r_t_n.html).

To enable SWMR for the HDF5 data writer, set the configuration variable `allow_concurrent_reading` to `True`. The default is `False`, disabling this feature.

This mode has the following benefits:

- allow to read the file while it is being written, and
- improve reliability: "[SWMR] has the added benefit of leaving a file in a valid state even if the writing application crashes before closing the file properly" (see [h5py docs](https://docs.h5py.org/en/stable/swmr.html)).

However, it also has a couple of requirements and drawbacks of which the most important are:

- no new objects (e.g. Groups, Datasets or Attributes) can be added to the file *after* SWMR mode has been enabled;
- data must be appended, and to avoid frequent modification, the Dataset will be resized in larger steps, leading to slightly larger files; and  
- a known issue with SWMR exists on Windows systems ([see corresponding issue](https://github.com/h5py/h5py/issues/2259)).

When enabling SWMR, all objects in the file will already be created at the start
of the run. When all `BOR` packets have been received from all connected
satellites, the file will be set to SWMR mode. After this, incoming data will be
appended to the `data` Dataset instead of new Datasets being created for each
individual package. See the example below for how to load data from files
written in this way.

## Example for loading data

The following example shows how to load data stored in SWMR mode (see Configuration section above).

``` python
import h5py
import numpy as np
import json

# open the h5 file:
fn = "my_data_file.h5"
h5file = h5py.File(fn)

# select the satellite of interest:
grp = h5file["simple_sender"]

# datasets we want to load from:
data = h5file["simple_sender"]["data"]
meta = h5file["simple_sender"]["meta"]

# datasets have been appended to, so we need to use the indices to retrieve
# individual packages:
data_idx = h5file["simple_sender"]["data_idx"]
meta_idx = h5file["simple_sender"]["meta_idx"]

# loop over each package:
prev_data = 0
prev_meta = 0
i = 0
for i in range(data_idx.shape[0]):
    # start with meta data!
    # this particular package's meta data ends at position:
    idx = meta_idx[i]
    if idx == 0:
        # reached the end of the received packages.
        break
    # slice the meta data out of the data set and convert back to dict:
    m = json.loads(str(meta[prev_meta:idx], encoding='utf-8'))
    # store current index value for next iteration
    prev_meta = idx

    # maybe we can determine the data type from the meta data?
    if "dtype" in m:
        dtype = m["dtype"]
    else:
        # use a default, adjust this to your actual dtype!
        dtype = "int16"

    # now load actual payload data:
    idx = data_idx[i]
    payload = np.array(data[prev_data:idx]).view(np.dtype(dtype))
    prev_data = idx
    print(f"Packet #{i} loaded with metadata '{m}' and payload '{payload}'")

```
