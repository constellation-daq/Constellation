# The Listener

Constellation listeners are passive observers that receive and process the stream of log messages and telemetry data emitted
by the other participants, without influencing the state of the Constellation in any way. Listeners usually are used for
monitoring of the Constellation, and they subscribe only to the shard of information they are interested in as sketched in
{numref}`fig-cmdp-schema`.

```{figure} CMDP.svg
:align: center
:name: fig-cmdp-schema
Schematic drawing of the Constellation logging & telemetry protocol
```

```{seealso}
A detailed technical description, including protocol sequence diagrams, can be found in the
[protocol description chapter](../../framework_reference/protocols.md#monitoring) in the framework development guide.
```

## Operating Principle

When a new listener is started, it connects to the running Constellation, discovers satellites in its group through the
[network discovery](./discovery.md) protocol, and subscribes to the log messages or metric data of interest.
It then processes those messages, e.g. for displaying them on screen, storing them to disk, or forwarding them to an
external service.

From the perspective of the running Constellation, a listener is invisible. It does not participate in the control protocol
and can neither send nor receive commands, does not participate in heartbeat exchange, and has no role in the satellite
finite state machine. Consequently, listeners can be started and stopped at any time, including while a run is in
progress, without any effect on the satellites or the data being acquired.

```{note}
Because listeners receive messages only while connected, log messages and metrics that were emitted before the respective
listener connected are not cached and retroactively delivered.
```

Typical Constellation listeners comprise user interfaces such as the graphical logging application
[Observatory](./logging.md#graphical-logger), which implements real-time display of log messages with per-sender subscription
and message filtering, or the chart dashboard [TelemetryConsole](./telemetry.md#real-time-dashboard), a real-time dashboard
for telemetry time series.

## Listeners and Satellites

In some cases, the functionality of a [satellite](./satellite.md) is combined with that of a listener. This allows them
to be configured through the standard configuration file, and can be managed by a controller just like any other satellite.
This means, the node partakes in the satellite finite state machine, change states as the data taking progresses, but in
addition subscribes to log or telemetry data streams and processed them.

Typical satellites which contain a listener component are interface satellites which relay information to external services
such as messengers or databases. For example the [Mattermost](../../satellites/Mattermost.md) satellite forwards log
messages to a *Mattermost* channel for remote monitoring, and the [Influx](../../satellites/Influx.md) satellite writes
telemetry data to an *InfluxDB* time-series database for persistent storage and visualization with *Grafana*.

The [FlightRecorder](../../satellites/FlightRecorder.md) archives log messages to disk and supports several storage
strategies, including per-run log files and daily rotation.
