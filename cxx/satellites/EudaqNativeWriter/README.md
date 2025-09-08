---
# SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
# SPDX-License-Identifier: CC-BY-4.0 OR EUPL-1.2
title: "EudaqNativeWriter"
description: "Satellite receiving data and writing it to EUDAQ2 native binary formatted files"
category: "Data Receivers"
---

## Description

This satellite receives data from other satellites and encodes them in the EUDAQ2 native binary file format.
The satellite can be used for compatibility when using analysis software that interfaces EUDAQ2 for decoding data.

Since Constellation and EUDAQ2 data transmission formats are slightly different, there are two BOR tags which *sending*
satellites should set in order to ensure the data can be correctly decoded:

* The sending satellite should set the `eudaq_event` BOR tag to a string describing the EUDAQ2 event type to be used for
  encoding, for example `TluRawDataEvent` for data sent by the AIDA TLU. If this tag is not set, the satellite falls back
  to using the *name* portion of the canonical name of the sender. This can be used to improve interoperability of
  satellites which do not set these tags explicitly.
* EUDAQ2 knows data blocks per event as well as sub-events, while Constellation data records always consist of a dictionary
  and any number of data blocks. In order to encode this data correctly to EUDAQ2 native binary format, the sending satellite
  should specify if the Constellation data blocks should be interpreted as as individual *data blocks* or as *sub-events* in
  the resulting EUDAQ2 native binary event by setting the BOR tag `write_as_blocks` to either `true` or `false`, respectively.
  If the tag is not provided, this satellite defaults to interpreting them as sub-events, repeating the dictionary for all of
  the attached blocks.

BOR and EOR messages which arrive before the start of a run and after its end are treated differently from regular data
messages. The corresponding EUDAQ events are marked as BORE and EORE, respectively. The header of the EUDAQ event will
contain all information from the corresponding Constellation data records. For the BOR, this is the user-provided tags as
well as the additional `EUDAQ_CONFIG` key containing a string representation of the satellite configuration. For the EOR, the
user-provided tags and the framework-provided metadata are merged into a single dictionary.

The data events can contain the following header flags which will be interpreted and translated to the corresponding EUDAQ
flags or event configurations:

* `flag_trigger` (boolean): if this flag is set to `true`, the corresponding EUDAQ `FLAG_TRIG` is set on the event. This will
  cause analysis software to treat the information as trigger information, i.e. use the trigger number and timestamp as
  compound information. This flag is necessary e.g. for the AIDA TLU.
* `trigger_number` (integer): If set, the trigger number of the current EUDAQ event will be set from this, if not set the
  Constellation message sequence is used instead.
* `timestamp_begin` (integer): Timestamp of the event start in units of picoseconds. If the tag is available from the
  Constellation message header, the value will be translated to nanoseconds and stored as EUDAQ event timestamp. If the tag
  is not set, `0` will be set as timestamp. This prompts analysis software to use the trigger number instead.
* `timestamp_begin` (integer): Timestamp of the event end in units of picoseconds. If the tag is available from the
  Constellation message header, the value will be translated to nanoseconds and stored as EUDAQ event timestamp. If the tag
  is not set, `0` will be set as timestamp. This prompts analysis software to use the trigger number instead.

Messages from all sending satellites are written into the output file sequentially in the order in which they arrive.
Output files are stored under the path provided via the `output_path` parameter and are named `data_<run_identifier>.raw`
where `<run_identifier>` is the identifier of the corresponding run.

## Building

This satellite does not have any external dependencies and is therefore built by default. Should this not be desired can the
build be deactivated via

```sh
meson configure build -Dsatellite_eudaq_native_writer=false
```

## Parameters

| Parameter | Type | Description | Default Value |
|-----------|------|-------------|---------------|
| `output_directory` | String | Base path to which to write output files to | - |
| `flush_interval` | Integer | Interval in seconds in which data should be flushed to disk | 3 |
