<!-- markdownlint-disable MD041 -->
### Parameters inherited from `TransmitterSatellite`

| Parameter | Type | Description | Default Value |
|-----------|------|-------------|---------------|
| `_bor_timeout` | Unsigned integer | Timeout in seconds to send the BOR message. The satellite will attempt for this interval to send the message and goes into `ERROR` state if it fails to do so. A possible reason for failure is that no receiver satellite connected to this satellite and is receiving data. | 10 |
| `_eor_timeout` | Unsigned integer |  Timeout in seconds to send the EOR message. The satellite will attempt for this interval to send the message and goes into `ERROR` state if it fails to do so. | 10 |
| `_data_timeout` | Unsigned integer | Timeout in seconds to send the data message. The satellite will attempt for this interval to send the message and goes into `ERROR` state if it fails to do so. | 10 |
| `_payload_threshold` | Unsigned integer | Threshold for sending data messages in KiB. The satellite will only send queued data records after the combined payload size of the data records has reached this threshold. | 128 |
| `_queue_size` | Unsigned integer | Size of the queue for the data records. Small values might lead to performance issues, large values lead to larger memory usage. | 32768 |
| `_data_license` | String | License this data is recorded under. Defaults to the [Open Data Commons Attribution License](https://opendatacommons.org/licenses/by/). This information will be added to the run metadata. | `ODC-By-1.0` |
