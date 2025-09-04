<!-- markdownlint-disable MD041 -->
### Metrics inherited from `ReceiverSatellite`

| Metric | Description | Value Type | Metric Type | Interval |
|--------|-------------|------------|-------------|----------|
| `RX_BYTES` | Amount of bytes received from all transmitters during current run | Integer | `LAST_VALUE` | 10s |
| `DISKSPACE_FREE` | Amount of megabytes available on the file system the current output file is located | Integer | `LAST_VALUE` | 10s |
| `OUTPUT_FILE`    | Path of the currently used output file for this satellite. Updated whenever it changes. | String | `LAST_VALUE` | - |
