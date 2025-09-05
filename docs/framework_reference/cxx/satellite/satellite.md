# Satellite

## Data

Data are sent via the [CDTP protocol](../../../protocols/cdtp.md), where data are send in *data records*, consisting of
blocks containing binary data. It is up to the implementation to decide how to organize the data. Additionally, each record
can contain metadata in form of tags (key-value pairs) in the record header.

### Transmitting Data

The {cpp:class}`TransmitterSatellite <constellation::satellite::TransmitterSatellite>` class can be used to send data during
a run over the network. It sends the Begin-of-Run (BOR) message in the `starting` state and the End-of-Run (EOR) message in
the `stopping` state. This is achieved in the {cpp:class}`BaseSatellite <constellation::satellite::BaseSatellite>` by
dynamically casting it to a {cpp:class}`TransmitterSatellite <constellation::satellite::TransmitterSatellite>`.

```{warning}
Sending data is not thread safe. If multiple threads need to access the sender, it needs to be protected with a mutex.
```

### Receiving Data

The {cpp:class}`ReceiverSatellite <constellation::satellite::ReceiverSatellite>` class can be used to receive data during
a run over the network. Similar to the {cpp:class}`TransmitterSatellite <constellation::satellite::TransmitterSatellite>`
mentioned above, executing transmitter transitions in addition to user transitions is achieve in the
{cpp:class}`BaseSatellite <constellation::satellite::BaseSatellite>` by dynamically casting it to a
{cpp:class}`ReceiverSatellite <constellation::satellite::ReceiverSatellite>`.

## `constellation::satellite` Namespace

```{doxygennamespace} constellation::satellite
:content-only:
:members:
:protected-members:
:undoc-members:
```
