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
:::{tab-item} PyPI
:sync: pypi

```sh
pip install "ConstellationDAQ[hdf5]"
```

:::
:::{tab-item} Source
:sync: source

```sh
pip install --no-build-isolation -e ".[hdf5]"
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

## Data Format

A group is created for each transmitter with its canonical name. In this group further groups are created for the begin-of-run (`BOR`) and end-of-run (`EOR`) messages, as well as a group for each data (`data_XXXXXXXXX`) message.

The BOR and EOR groups contain two empty datasets, one called `user_tags` containing the user provided tags for those message, and one called `configuration` for the BOR or `run_metadata` for the EOR respectively. The configuration and run metadata are added as attributes to those datasets.

Each data group contains the tags of the data record as attributes and contains a dataset for each block contained in the data record (`block_XX`). By default data is stored with the `uint8` data type.
The data type might differ if the data record contained a `dtype` tag, in which case `numpy` is used to extract store data with the given data type.

Additionally, a group for the receiver is created. It contains three attributes: `constellation_version`, `date_utc` and `swmr_mode`.

## SWMR Mode

Usually, it is not safe to read from an HDF5 file while data is being written to it. However, a special writing mode available in recent versions of HDF5 allows such concurrent reading: ["Single-Writer Multiple-Reader" (SWMR) mode](https://support.hdfgroup.org/documentation/hdf5/latest/_s_w_m_r_t_n.html).

If the SWMR mode is enabled via the `allow_concurrent_reading` parameter, the data format differs due to certain limitations of the SWMR mode.

```{important}
In SWMR mode no new datasets can be created, thus all datasets for a transmitter need to be created as soon the BOR has arrived. All data transmitters have to be specified explicitly via the `_data_transmitters` parameter.
```

Once all BOR messages have been received, the file will be set to SWMR mode.
After this, incoming data will be appended to the `data` dataset instead of new datasets being created for each data record.
Individual data records can be separated using the `data_idx` dataset, which contains the indices where the data record ends.
Similarly, metadata contained in the data records will be appended to the `meta` dataset encoded as json, with indices stored in the `meta_idx` dataset.
If a data record contains multiple block, the are appended and a `block_lengths` entry is added the the metadata, which can be used to extract the individual blocks.

EOR messages are encoded as JSON instead of using attributes.

### Python Decoder

The following Python code can be used to dump the content of a file to the console:

``` python
import json
import h5py
import numpy as np

# Open the h5 file
file_name = "my_data_file.h5"
h5file = h5py.File(file_name)

# Select group of satellite of interest
sender = "PyRandomTransmitter.sender"
grp = h5file[sender]

# Get datasets
dset_data = h5file[sender]["data"]
dset_meta = h5file[sender]["meta"]
dset_data_idx = h5file[sender]["data_idx"]
dset_meta_idx = h5file[sender]["meta_idx"]

# Loop over each data record
prev_data = 0
prev_meta = 0
i = 0
for i in range(dset_data_idx.shape[0]):
    # Start with metadata, extract end position
    idx = dset_meta_idx[i]
    if idx == 0:
        # Reached the end
        break
    # Slice the metadata out of the dataset and convert back to dict
    tags = json.loads(str(dset_meta[prev_meta:idx], encoding='utf-8'))
    # Store current index value for next iteration
    prev_meta = idx

    # Maybe we can determine the data type from the metadata?
    dtype = np.dtype(tags.get("dtype", "uint8"))

    # Now load actual data
    idx = dset_data_idx[i]
    data_raw = np.split(np.array(dset_data[prev_data:idx], dtype=np.uint8), np.cumsum(tags["block_lengths"])[:-1])
    data = [raw_block.view(dtype) for raw_block in data_raw]
    prev_data = idx
    print(f"Data record #{i} loaded with metadata {tags} and data {data}")
```

## Parameters

| Parameter | Type | Description | Default Value |
|-----------|------|-------------|---------------|
| `output_directory` | String | Base path to which to write output files to | - |
| `flush_interval` | Float | Interval in seconds controlling how often to flush the file | `10.0` |
| `allow_concurrent_reading` | Bool | Enable concurrent reading (aka SWMR mode) | `False` |

## Metrics

| Metric | Description | Value Type | Metric Type | Interval |
|--------|-------------|------------|-------------|----------|
| `CONCURRENT_READING_ENABLED` | Concurrent reading status | Bool | `LAST_VALUE` | 5s |

## Custom Commands

| Command | Description | Arguments | Return Value |
|---------|-------------|-----------|--------------|
| `get_concurrent_reading_status` | Get if concurrent reading is enabled | - | Bool |
