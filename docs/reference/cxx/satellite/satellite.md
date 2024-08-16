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

## `constellation::satellite` Namespace

```{doxygennamespace} constellation::satellite
:content-only:
:members:
:protected-members:
:undoc-members:
```
