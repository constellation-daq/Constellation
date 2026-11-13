# Sending & Receiving Data

The following tutorial describes how data are transmitted from one [satellite](../concepts/satellite.md) to another. It
builds on the [Starting & Controlling a Satellite](./single_satellite.md) tutorial, which introduced the starting of
satellites and their control via the interactive command line controller. In this tutorial, a transmitter satellite
generating random data and a receiver satellite accepting these data are started, configured and operated.

```{hint}
This tutorial only requires the Python implementation of Constellation. If the C++ implementation is installed, the
`PyRandomTransmitter` and `PyDevNullReceiver` satellites can be replaced with their C++ counterparts `RandomTransmitter` and
`DevNullReceiver`, respectively. All appearances of satellite types in commands and configuration have to be replaced
accordingly.
```

## Data Transmission in Constellation

Data in Constellation are handled by two types of satellites: transmitter satellites generate data and send them through
Constellation, and receiver satellites receive data from one or more transmitters and process them, e.g. by storing them to
disk.

Data are grouped into runs. A run starts when the satellites enter the {bdg-secondary}`RUN` state and ends with the
departure from this state, normally back to {bdg-secondary}`ORBIT`. Each run is labeled by a run identifier which is
provided with the `start` command. The first message of each run is the begin-of-run (BOR) message, which contains the
configuration of the transmitting satellite, followed by any number of data messages and concluded by the end-of-run (EOR)
message containing run metadata.

```{seealso}
A detailed description of the data structure, message types and run condition flags can be found in the
[Data Processing](../concepts/data.md) concepts section.
```

## Starting the Satellites

In this tutorial, the `PyRandomTransmitter` satellite is used as transmitter. It generates blocks of random bytes and serves
as an example to generate data without requiring any instrument hardware. It is started in the same way as the satellite in
the previous tutorial, providing a name and the Constellation group:

```sh
SatellitePyRandomTransmitter -n Sender -g edda
```

As receiver, a satellite is used which accepts all incoming data and subsequently discards them. Since no data are written
to disk, neither an output location needs to be configured nor is any disk space consumed. The receiver satellite is started
in a second terminal and placed in the same Constellation group:

```sh
SatellitePyDevNullReceiver -n Receiver -g edda
```

```{note}
Several transmitter and receiver satellites can be operated within the same Constellation. Receiver satellites storing
data to disk in different file formats are listed in the *Data Receivers* category of the
[Satellite Library](../../satellites/index.md).
```

## Configuring the Satellites

The data transmission is configured via the configuration file, which is passed to the satellites with the `initialize`
command. The following configuration file, stored as `data.toml`, provides settings for both satellites:

```toml
[PyRandomTransmitter.Sender]
# Size of each data block in bytes and number of blocks per data record
block_size = 1024
number_of_blocks = 1

[PyDevNullReceiver.Receiver]
# Transmitter satellites this receiver should receive data from
_data.receive_from = ["PyRandomTransmitter.Sender"]
```

The `_data.receive_from` parameter holds a list of canonical names of transmitter satellites the receiver should connect
to. If this parameter is omitted, the receiver connects to all transmitters present in the Constellation. Parameters
starting with an underscore indicate framework parameters interpreted by Constellation itself rather than by the individual
satellite implementation.

```{seealso}
All parameters of the satellites used in this tutorial are described on the pages of the
[PyRandomTransmitter](../../satellites/PyRandomTransmitter.md) ([RandomTransmitter](../../satellites/RandomTransmitter.md)
for C++) and [PyDevNullReceiver](../../satellites/PyDevNullReceiver.md)
([DevNullReceiver](../../satellites/DevNullReceiver.md) for C++) satellites. The general structure of configuration files is
detailed in the [Configuration Files](../concepts/configuration_files.md) section.
```

## Taking Data

This section takes the started Constellation through a full data taking cycle, initializing and launching the satellites,
starting a new run, and stopping and landing subsequently.

### Initializing and Launching

The controller is started with the configuration file passed as command line argument, as described in the
[previous tutorial](./single_satellite.md#loading-a-configuration-file):

```sh
Controller -g edda --config data.toml
```

Both satellites should be listed as connected:

```python
edda > constellation.satellites
{
    "PyDevNullReceiver.Receiver": SatelliteCommLink(type=PyDevNullReceiver, name=Receiver),
    "PyRandomTransmitter.Sender": SatelliteCommLink(type=PyRandomTransmitter, name=Sender),
}
```

The satellites are subsequently initialized with the loaded configuration and launched into the {bdg-secondary}`ORBIT`
state:

```python
edda > constellation.initialize(cfg)
{
    "PyDevNullReceiver.Receiver": SatelliteResponse(msg="transitioning", payload=initialize),
    "PyRandomTransmitter.Sender": SatelliteResponse(msg="transitioning", payload=initialize),
}

edda > constellation.launch()
{
    "PyDevNullReceiver.Receiver": SatelliteResponse(msg="transitioning", payload=launch),
    "PyRandomTransmitter.Sender": SatelliteResponse(msg="transitioning", payload=launch),
}
```

Whether both satellites have reached the {bdg-secondary}`ORBIT` state can be verified via:

```python
edda > constellation.get_state()
{
    "PyDevNullReceiver.Receiver": SatelliteResponse(msg="ORBIT", payload=48, meta={...}),
    "PyRandomTransmitter.Sender": SatelliteResponse(msg="ORBIT", payload=48, meta={...}),
}
```

### Starting a Run

A run is started by sending the `start` command with a run identifier to the entire Constellation. The run identifier is a
free-form string composed of alphanumeric characters, underscores or dashes:

```python
edda > constellation.start("run_0001")
{
    "PyDevNullReceiver.Receiver": SatelliteResponse(msg="transitioning", payload=start),
    "PyRandomTransmitter.Sender": SatelliteResponse(msg="transitioning", payload=start),
}
```

Upon entering the {bdg-secondary}`RUN` state, the transmitter sends the BOR message to the receiver and subsequently
starts generating and transmitting data records. The receiver accepts all incoming messages, keeps track of the amount of
received data and discards the data afterwards.

```{note}
The transmitter requires a connected receiver in order to send the BOR message. If no receiver accepts the message within
the time configured via the `_data.bor_timeout` parameter, the transmitter satellite enters the {bdg-secondary}`ERROR`
state. This is the case e.g. when the receiver is not running or when its `_data.receive_from` parameter does not contain
the canonical name of the transmitter.
```

### Monitoring the Data Flow

The amount of transmitted and received data is published by the satellites as [telemetry](../concepts/telemetry.md)
metrics. Transmitter satellites provide the `TX_BYTES` and `TX_RECORDS` metrics, while receiver satellites provide the
`RX_BYTES` metric. These metrics can be displayed with the [Command-Line Monitor](./cli_monitor.md), which is started in an
additional terminal:

```sh
Monitor -g edda --metrics TX_BYTES TX_RECORDS RX_BYTES
```

The metrics are updated every few seconds and printed to the terminal, which allows to verify that data are transmitted
and received during the run.

```{seealso}
Telemetry data can also be displayed graphically, e.g. using the [TelemetryConsole](./telemetryconsole.md) or a
[Grafana dashboard](../howtos/setup_influxdb_grafana.md).
```

### Stopping the Run

The run is ended by sending the `stop` command:

```python
edda > constellation.stop()
{
    "PyDevNullReceiver.Receiver": SatelliteResponse(msg="transitioning", payload=stop),
    "PyRandomTransmitter.Sender": SatelliteResponse(msg="transitioning", payload=stop),
}
```

When stopping the run, the transmitter sends the EOR message containing run metadata. The receiver waits for the EOR messages
of all connected transmitters before returning to the {bdg-secondary}`ORBIT` state. If an EOR message is not received
within the time configured via the `_data.eor_timeout` parameter, the receiver satellite enters the {bdg-secondary}`ERROR`
state.

Further runs can be taken by repeating the `start(...)` and `stop()` commands with new run identifiers.

## Landing & Shutting Down

After data taking has been concluded, the satellites can be landed and subsequently shut down:

```python
edda > constellation.land()
{
    "PyDevNullReceiver.Receiver": SatelliteResponse(msg="transitioning", payload=land),
    "PyRandomTransmitter.Sender": SatelliteResponse(msg="transitioning", payload=land),
}

edda > constellation.shutdown()
{
    "PyDevNullReceiver.Receiver": SatelliteResponse(msg="PyDevNullReceiver.Receiver queued for reentry"),
    "PyRandomTransmitter.Sender": SatelliteResponse(msg="PyRandomTransmitter.Sender queued for reentry"),
}
```
