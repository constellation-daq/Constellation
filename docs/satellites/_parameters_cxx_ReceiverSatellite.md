<!-- markdownlint-disable MD041 -->
### Parameters inherited from `ReceiverSatellite`

Parameters to control [data transmission](../operator_guide/concepts/data.md) in the `_data` section:

| Parameter | Type | Description | Default Value |
|-----------|------|-------------|---------------|
| `file_conflict_strategy` | String | Strategy for output file name conflicts. Possible values are `ERROR`, `RENAME` and `OVERWRITE`. When set to `ERROR`, this satellite will go into `ERROR` state if the target file exists. When set to `RENAME`, a randomly generated 8-character Base36 hash such as `3md7ku73` is appended to the target file name. When set to `OVERWRITE`, the existing file or directory is removed before creating the target file. | `RENAME` |
| `receive_from` | List of strings | List of canonical names of transmitter satellites this receiver should connect to and receive data messages from. If empty, this receiver will connect to all transmitters. | `[]` |
| `eor_timeout` | Unsigned integer | Timeout waiting for the reception of the end-of-run message. The receiver satellite will wait this number of seconds for receiving the EOR message from each connected transmitter satellite, and will go into error state if the message has not been received within this period. The timeout will only be started after the pending data messages have been read from the queue. | `10` |
