# Satellite

## Data

Data is sent via the [CDTP protocol](../../../protocols/cdtp.md), which send data in messages that can contain several
*data frames* (also called message payload frames). Since data is sent as binary data, it is up to the implementation to
decide how to organize the data. Additionally, each message can contain metadata in form of tags (key-value pairs) in the
message header.

### Transmitting Data

The {cpp:class}`TransmitterSatellite <constellation::satellite::TransmitterSatellite>` class can be used to send data during
a run over the network. It sends the Begin-of-Run (BOR) message in the `starting` state and the End-of-Run (EOR) message in
the `stopping` state. This is achieved in the {cpp:class}`BaseSatellite <constellation::satellite::BaseSatellite>` by
dynamically casting it to a {cpp:class}`TransmitterSatellite <constellation::satellite::TransmitterSatellite>`.

```{warning}
Sending data is not thread safe. If multiple threads need to access the sender, it needs to be protected with a mutex.
```

#### `TransmitterSatellite` configuration parameters

| `_bor_timeout` | Unsigned integer | Timeout in seconds to send the BOR message. The satellite will attempt for this interval to send the message and goes into `ERROR` state if it fails to do so. A possible reason for failure is that no receiver satellite connected to this satellite and is receiving data. | 10 |
| `_eor_timeout` | Unsigned integer |  Timeout in seconds to send the EOR message. The satellite will attempt for this interval to send the message and goes into `ERROR` state if it fails to do so. | 10 |
| `_data_timeout` | Unsigned integer | Timeout in seconds to send the data message. The satellite will attempt for this interval to send the message and goes into `ERROR` state if it fails to do so. | 10 |
| `_data_license` | String | License this data is recorded under. Defaults to the [Open Data Commons Attribution License](https://opendatacommons.org/licenses/by/). This information will be added to the run metadata. | `ODC-By-1.0` |

### Receiving Data

The {cpp:class}`ReceiverSatellite <constellation::satellite::ReceiverSatellite>` class can be used to receive data during
a run over the network. Similar to the {cpp:class}`TransmitterSatellite <constellation::satellite::TransmitterSatellite>`
mentioned above, executing transmitter transitions in addition to user transitions is achieve in the
{cpp:class}`BaseSatellite <constellation::satellite::BaseSatellite>` by dynamically casting it to a
{cpp:class}`ReceiverSatellite <constellation::satellite::ReceiverSatellite>`.

#### `ReceiverSatellite` configuration parameters

| Parameter | Type | Description | Default Value |
|-----------|------|-------------|---------------|
| `_allow_overwriting` | Bool | Switch whether overwriting files is allowed or not. If set to `false` and a file exists already, this satellite will go into `ERROR` state. | `false` |
| `_data_transmitters` | List of strings | List of canonical names of transmitter satellites this receiver should connect to and receive data messages from. If empty, this receiver will connect to all transmitters. | `[]` |
| `_eor_timeout` | Unsigned integer | Timeout waiting for the reception of the end-of-run message. The receiver satellite will wait this number of seconds for receiving the EOR message from each connected transmitter satellite, and will go into error state if the message has not been received within this period. The timeout will only be started after the pending data messages have been read from the queue. | `10` |

## `constellation::satellite` Namespace

```{doxygennamespace} constellation::satellite
:content-only:
:members:
:protected-members:
:undoc-members:
```
