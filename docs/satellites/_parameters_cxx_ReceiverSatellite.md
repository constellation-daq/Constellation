<!-- markdownlint-disable MD041 -->
### Parameters inherited from `ReceiverSatellite`

| Parameter | Type | Description | Default Value |
|-----------|------|-------------|---------------|
| `_allow_overwriting` | Bool | Switch whether overwriting files is allowed or not. If set to `false` and a file exists already, this satellite will go into `ERROR` state. | `false` |
| `_data_transmitters` | List of strings | List of canonical names of transmitter satellites this receiver should connect to and receive data messages from. If empty, this receiver will connect to all transmitters. | `[]` |
| `_eor_timeout` | Unsigned integer | Timeout waiting for the reception of the end-of-run message. The receiver satellite will wait this number of seconds for receiving the EOR message from each connected transmitter satellite, and will go into error state if the message has not been received within this period. The timeout will only be started after the pending data messages have been read from the queue. | `10` |
