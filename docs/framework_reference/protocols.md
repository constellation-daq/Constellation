# Communication Protocols

Constellation is built around a set of communication protocols among its constituents. These protocols are well-defined and have been defined
early on and serve as platform- and implementation-independent architecture of the framework, meaning that new implementations of e.g. a satellite can be written in any language.
The communication channels are independent of each other and follow clear communication patterns such as publish/subscribe for one-to-many distribution of information
or request/reply for a client-server-based communication.

Most of the protocols are TCP/IP communication based on the [ZeroMQ messaging library](https://zeromq.org/) and build upon the ZeroMQ Message Transport Protocol.
The protocols are documented in RFC-style documents, including an [ABNF description](https://en.wikipedia.org/wiki/Augmented_Backus%E2%80%93Naur_form)
where relevant, and can be found in the appendix of this manual.

The five Constellation communication protocols are described in the following, ordered by significance of the information passed.

## Autonomous Operation

Autonomous operation of the Constellation requires constant exchange of state information between all participants. This is
implemented via heartbeats send via the Constellation Heartbeat Protocol (CHP). Heartbeat messages contain

* A timestamp when the heartbeat was sent
* The current state of the sender finite state machine
* A set of flags indicating the desired treatment of the sender
* The time interval after which the next heartbeat is to be expected.

With this information, heartbeat receivers can deduce independently whether a remote system is in {bdg-secondary}`ERROR` state
or not responding due to network or machine failure, and how to react to this information.

In addition to regular heartbeat patterns, so-called extrasystoles are sent out-of-order whenever the state of the sender
changes. This enabled immediate reaction to remote state changes without having to wait for the next regular heartbeat update
interval.

Be low are two sequence diagrams illustrating the heartbeat exchange, once for the communication between a controller instance and a satellite,
and once for the heartbeat exchange between two satellites. Extrasystole messages that are sent at state changes are indicated in red, the activation
bars indicate times of active heartbeat monitoring of the remote satellite.

::::{grid} 1 1 2 2

:::{grid-item-card}
**Heartbeats with Controller & Satellite**
^^^^^^^^^^^^

```plantuml
@startuml
skinparam ParticipantPadding 50
note over "Controller" : Controller joins
"Satellite" <-- "Controller": Subscription
activate "Controller" #lightblue
"Satellite" -> "Controller": Heartbeat **[launching]**
"Satellite" -[#lightcoral]> "Controller": <font color=lightcoral>Extrasystole</font> **[ORBIT]**
"Satellite" -> "Controller": Heartbeat **[ORBIT]**
|||
"Satellite" -> "Controller": Heartbeat **[ORBIT]**
note over "Controller" : Controller departs
"Satellite" <-- "Controller": Unsubscription
deactivate "Controller"
@enduml
```

:::

:::{grid-item-card}
**Heartbeats between Satellites**
^^^^^^^^^^^^

```plantuml
@startuml
skinparam ParticipantPadding 50
note over "Satellite A" : Satellite joins
"Satellite A" --> "Satellite B": Subscription
activate "Satellite A" #lightblue
"Satellite A" <-- "Satellite B": Subscription
activate "Satellite B" #lightblue
"Satellite A" <- "Satellite B": Heartbeat **[launching]**
"Satellite A" -> "Satellite B": Heartbeat **[NEW]**
"Satellite A" <[#lightcoral]- "Satellite B": <font color=lightcoral>Extrasystole</font> **[ORBIT]**
"Satellite A" <- "Satellite B": Heartbeat **[ORBIT]**
"Satellite A" -> "Satellite B": Heartbeat **[NEW]**
note over "Satellite B" : Satellite departs
"Satellite A" <-- "Satellite B": Unsubscription
deactivate "Satellite B"
"Satellite A" --> "Satellite B": Unsubscription
deactivate "Satellite A"
@enduml
```

:::

::::


## Command & Controlling

Commands from controller instances to satellites are transmitted via the Constellation Satellite Control Protocol (CSCP). It
resembles a client-server architecture with the typical request-reply pattern. Here, the satellite acts as the server while
the controller assumes the role of the client.

::::{grid}

:::{grid-item-card}
**Command Communication between Controller & Satellite**
^^^^^^^^^^^^

```plantuml
@startuml
skinparam ParticipantPadding 50
"Satellite A" <-- "Controller": Connect
"Satellite A" <- "Controller": **REQUEST** get_name
"Satellite A" -[#skyblue]> "Controller": <font color=skyblue><b>SUCCESS</b></font> Satellite A
|||
"Satellite A" <- "Controller": **REQUEST** unknown_function
"Satellite A" -[#lightcoral]> "Controller": <font color=lightcoral><b>UNKNOWN</b></font> Unknown command
|||
"Satellite A" <- "Controller": **REQUEST** start "run_1"
"Satellite A" -[#lightcoral]> "Controller": <font color=lightcoral><b>INVALID</b></font> Invalid transition from "INIT"

"Satellite A" <-- "Controller": Disconnect
@enduml
```

:::

::::


## Data Transmission

Data are transferred within a Constellation network using the Constellation Data Transmission Protocol (CDTP). It uses
point-to-point connections via TCP/IP, which allow the bandwidth of the network connection to be used as efficiently as
possible. The message format transmitted via CDTP is a lightweight combination of the sender's name, the message type and any
amount of data records. A data record consists of a sequence number, a dictionary and any number of data blocks.

CDTP knows three different message types:

* `BOR` - Begin of Run: This message is sent automatically at the start of a new measurement, i.e. upon entering the `RUN`
  state of the finite state machine of the sending satellite. It marks the start of a measurement in time and contains
  exactly two data records. The dictionary of the second record contains the configuration of the sending satellite, while
  the dictionary in first record might contain additional information of the sending satellite not contained in the
  configuration.
* `DATA`: This is the standard message type of CDTP. The data record contain undecoded raw data of the respective instrument
  and the message sequence counts up with every message (starting from 1), providing additional possibility for checking data
  integrity offline.
* `EOR` - End of Run: This message is sent automatically at the end of a measurement, i.e. upon leaving of the `RUN` state by
  the sending satellite. It contains exactly two data records. The dictionary of the second record contains the run metadata
  collected by the framework, while the dictionary in first record might contain additional information of the sending
  satellite not in the run metadata generated by the framework.

Only `DATA` messages can be transmitted by satellite implementations, both `BOR` and `EOR` messages are handled automatically.

::::{grid}

:::{grid-item-card}
**Data Transmission Sequence**
^^^^^^^^^^^^

```plantuml
@startuml
skinparam ParticipantPadding 50
note across : Run is started
"Satellite A" <-- "Satellite B": Connect
"Satellite A" -> "Satellite B": **BOR** [Satellite Configuration]
activate "Satellite A" #lightcoral
activate "Satellite B" #lightcoral
loop #lightblue
    "Satellite A" -> "Satellite B": **DATA** [Instrument Data]
end
note across : Run is stopped
"Satellite A" -> "Satellite B": **EOR** [Run Metadata]
deactivate "Satellite A"
deactivate "Satellite B"
"Satellite A" <-- "Satellite B": Disconnect
@enduml
```

:::

::::


## Monitoring

The distribution of log messages and performance metrics within Constellation is handled by the Constellation Monitoring Distribution Protocol (CMDP).
The protocol is built around publisher and subscriber sockets which allow one-to-many distribution of messages. Subscriptions to logging levels
or metrics are completely independent of the current FSM state and can be performed at any time. This means that hosts listening and displaying
e.g. log messages can be ended and restarted and the subscriptions can be changed while the Constellation is running undisturbed.
The protocol features message filtering for data efficiency and minimal bandwidth usage. This means that a host only sends messages over the
protocol for which a subscription is present.

The same protocol is used for log messages and performance metrics. The following log levels are defined:

* `TRACE` messages are be used for verbose information which allows to follow the program flow for development purposes. This concerns, for example, low-level code for network communication or internal states of the finite state machine. The messages of this level also contain additional information about the code location of the program where the message has been logged from.
* `DEBUG` messages contain information mostly relevant to developers for debugging the program.
* `INFO` messages are of interest to end users and should contain information on the program flow of the component from a functional perspective. This comprises, e.g. reports on the progress of configuring devices.
* `WARNING` messages indicate unexpected events which require further investigation by the user.
* `STATUS` messages are used communicate important information on a low frequency such as successful state transitions.
* `CRITICAL` messages notify the end user about critical events which require immediate attention. These events may also have triggered an automated response and state change by the sending host.

The CMDP protocol support subtopics, which are appended to the log level. This allows to select only the relevant slice of information from an otherwise verbose
log level and therefore reduce the network bandwidth required. An example would be selecting only the `TRACE` messages relevant for network communication by
subscribing to the topic `TRACE/NETWORKING`.

Apart from log messages, CMDP is also used to transmit performance metrics such as the current trigger rate, the number of recorded events, temperature or CPU loads.
These messages are published under their respective topics and subscribers can choose the variables they want to follow.

::::{grid}

:::{grid-item-card}
**Subscription to Log Topics**
^^^^^^^^^^^^

```plantuml
@startuml
skinparam ParticipantPadding 50
"Satellite A" <-- "Listener": Connect
"Satellite A" <- "Listener": **SUBSCRIBE** LOG/WARNING
activate "Satellite A" #lightcoral
loop #lightblue
    "Satellite A" -> "Listener": **LOG/**WARNING [Message]
end

"Satellite A" <- "Listener": **SUBSCRIBE** LOG/INFO
activate "Satellite A" #gold

loop #lightblue
    "Satellite A" -> "Listener": **LOG/**INFO [Message]
    "Satellite A" -> "Listener": **LOG/**WARNING [Message]
end

"Satellite A" <- "Listener": **UNSUBSCRIBE** LOG/INFO
deactivate "Satellite A"
"Satellite A" <- "Listener": **UNSUBSCRIBE** LOG/WARNING
deactivate "Satellite A"
"Satellite A" <-- "Listener": Disconnect
@enduml
```

:::

::::

### CMDP Extension Types for Structured Metrics

The standard CMDP metric payload carries scalar values (integers, floats, strings, booleans, timestamps) encoded as native
MsgPack types. For data quality monitoring, CMDP is extended with application-specific MsgPack Extension types that carry
structured 2D data such as matrices and histograms.

These types use the MsgPack Extension format, which stores a tuple of an integer type code and a byte array payload.
MsgPack reserves type codes `[0, 127]` for application-defined use.

#### Extension Type Registry

| Type          | Code | Description                                       |
|---------------|------|---------------------------------------------------|
| Matrix        | 1    | Dense 2D array of typed numeric values            |
| Histogram1D   | 2    | 1D histogram with uniform binning                 |
| Histogram2D   | 3    | 2D histogram with uniform x and y binning         |

#### Element Data Types

Matrix elements carry a dtype tag that identifies the element type and its byte size.

| dtype tag | Type    | Size (bytes) | Description                    |
|-----------|---------|--------------|--------------------------------|
| 0         | uint32  | 4            | 32-bit unsigned integer        |
| 1         | uint64  | 8            | 64-bit unsigned integer        |
| 2         | float32 | 4            | IEEE 754 single precision      |
| 3         | float64 | 8            | IEEE 754 double precision      |

Histogram1D bin counts are always float64.

#### Byte Order

All multi-byte numeric fields inside ext payloads use **little-endian** byte order. This matches x86 native order and NumPy
defaults, enabling zero-copy deserialization on the most common hardware. The MsgPack framing itself (ext headers, length
fields) remains big-endian as defined by the MsgPack specification.

#### Matrix Format (ext type 1)

Matrix stores a dense 2D array of typed elements in row-major order. The ext payload layout is:

```text
+--------+--------+--------+--------+--------+--------+--------+--------+--------+========+
| dtype  |             rows (uint32 LE)      |             cols (uint32 LE)      |  data  |
+--------+--------+--------+--------+--------+--------+--------+--------+--------+========+
```

where:

* `dtype` is a uint8 element type tag
* `rows` is a uint32 little-endian row count
* `cols` is a uint32 little-endian column count
* `data` is `rows * cols * sizeof(dtype)` bytes of element data in row-major little-endian order
* Total payload size is `1 + 4 + 4 + rows * cols * sizeof(dtype)` bytes
* Element at `(row, col)` is at byte offset `(row * cols + col) * sizeof(dtype)` within `data`
* An empty matrix (0 rows or 0 columns) has a payload of exactly 9 bytes (no data section)

#### Histogram1D Format (ext type 2)

Histogram1D stores bin counts with uniform bin edges. The ext payload layout is:

```text
+--------+--------+--------+--------+--------+--------+--------+--------+
|                          start (float64 LE)                           |
+--------+--------+--------+--------+--------+--------+--------+--------+
|                          end (float64 LE)                             |
+--------+--------+--------+--------+--------+--------+--------+--------+
|          nbins (uint32 LE)        |
+--------+--------+--------+--------+
|              bins (nbins * float64 LE)                                |
+========+
```

where:

* `start` is a float64 little-endian lower edge of the first bin
* `end` is a float64 little-endian upper edge of the last bin
* `nbins` is a uint32 little-endian number of bins
* `bins` is `nbins * 8` bytes of float64 little-endian bin counts
* Total payload size is `8 + 8 + 4 + nbins * 8` bytes
* Bin `i` (0-indexed) spans `[start + i * (end - start) / nbins, start + (i+1) * (end - start) / nbins)`
* Underflow and overflow bins are not transmitted

#### Histogram2D Format (ext type 3)

Histogram2D combines axis metadata with an embedded Matrix ext blob. The ext payload layout is:

```text
+--------+--------+--------+--------+--------+--------+--------+--------+
|                          x_start (float64 LE)                         |
+--------+--------+--------+--------+--------+--------+--------+--------+
|                          x_end (float64 LE)                           |
+--------+--------+--------+--------+--------+--------+--------+--------+
|                          y_start (float64 LE)                         |
+--------+--------+--------+--------+--------+--------+--------+--------+
|                          y_end (float64 LE)                           |
+--------+--------+--------+--------+--------+--------+--------+--------+
|                     matrix (MsgPack ext type 1)                       |
+========+
```

where:

* `x_start`, `x_end`, `y_start`, `y_end` are float64 little-endian axis bin edges
* `matrix` is a complete MsgPack ext object (type 1) containing the bin count matrix
* The x axis has `matrix.cols` bins, the y axis has `matrix.rows` bins
* Matrix element at `(row, col)` holds the bin count for y bin `row` and x bin `col`
* Total payload size is `8 + 8 + 8 + 8 + sizeof(matrix ext object)` bytes

#### CMDP Metric Payload Integration

The existing CMDP metric payload consists of three consecutive MsgPack objects:

```text
+~~~~~~~~~~~~~~~~~+~~~~~~~~~~~~~~~~~+~~~~~~~~~~~~~~~~~+
|     value       |     flags       |      unit       |
+~~~~~~~~~~~~~~~~~+~~~~~~~~~~~~~~~~~+~~~~~~~~~~~~~~~~~+
```

For scalar metrics, `value` is a standard MsgPack type (int, float, str, etc.). For DQM metrics, `value` is a MsgPack
Extension object with type code 1, 2, or 3. Deserializers distinguish between them by checking whether the first MsgPack
object is an Extension type and dispatching on the type code.

Existing scalar metrics are unaffected. Listeners that do not understand the new extension types will encounter an unknown
MsgPack ext object in the value position and should skip it gracefully.


## Network Discovery

A common nuisance in volatile networking environments with devices appearing and disappearing is the discovery of available devices and services.
While some established protocols exist for the purpose of finding services on a local network, such as zeroconf or avahi, these come with significant
downsides such as missing standard implementations, being limited to individual platforms, or a large and complex set of features not required for the
purpose of Constellation.

Hence, the Constellation Host Identification & Reconnaissance Protocol (CHIRP) has been devised. It is a IPv4 protocol
intended to be used on local networks only which uses a set of defined beacons sent as multicast messages over UDP/IP to
announce or request services. The beacon message contains a unique identified for the host and its Constellation group, the
relevant service as well as IP address and port of the service. Three such beacons exist:

* `OFFER`: A beacon of this type indicates that the sending host is offering the service at the provided endpoint.
* `REQUEST`: This beacon solicits offers of the respective service from other hosts.
* `DEPART`: A departing beacon is sent when a host ceases to offer the respective service.

Each service the participating Constellation host offers is registered with its CHIRP service. Upon startup of the program, a `OFFER` beacon is sent
for each of the registered services.
The `REQUEST` beacon allows hosts to join late, i.e. after the initial `OFFER` beacons have been distributed. This means that at any time of the
framework operation, new hosts can join and request information on a particular service from the already running Constellation participants.
A clean shutdown of services is possible with the `DEPART` beacon which will prompt other hosts to disconnect.

::::{grid}

:::{grid-item-card}
**Discovery Message Communication**
^^^^^^^^^^^^

```plantuml
@startuml
skinparam ParticipantPadding 50
note over "Satellite A" : Satellite joins
"Satellite A" -> "Satellite B": **OFFER** [SERVICE]
"Satellite A" -> "Satellite B": **REQUEST** [SERVICE]
"Satellite A" <- "Satellite B": **OFFER** [SERVICE]
note over "Satellite A" : Satellite departs
"Satellite A" -> "Satellite B": **DEPART** [SERVICE]
@enduml
```

:::

::::
