# Using Observatory

Observatory is a graphical listener for Constellation. This tutorial demonstrates how to use Observatory to receive log
message from multiple satellites and how to filter for specific log messages.

```{seealso}
It is recommend to read through the tutorial on how to [use MissionControl](./missioncontrol.md) first.
```

## Starting Observatory

Observatory is started using the `Observatory` command or by searching for it in the application overview if installed
system-wide. On startup, the group name of the Constellation which should be controlled needs to be provided.

```{figure} qtgui_startup.png
:scale: 50 %
Observatory startup window
```

```{hint}
Alternatively, Observatory can be started with a group directly using the `-g GROUP` command line argument.
```

The main window of Observatory can be divided into three parts:

- A filter section on top to search through and filter already received messages in the interface
- A subscription section on the right to select which log messages to receive
- A list of all log messages

```{figure} observatory_empty.png
Observatory main window without log messages
```

## Initializing the Constellation

In order to control satellites, some satellites need to be started as part of the same group. In this tutorial,
two `Sputnik` satellites named `One` and `Two`, a `RandomTransmitter` named `Sender` and a `EudaqNativeWriter`
named `Receiver` are started.

```{figure} observatory_new.png
Observatory main window after satellites are started
```

To initialize the satellites, MissionControl can be used with the following configuration file:

```toml
[satellites.Sputnik.One]
unused_parameter = 1

[satellites.Sputnik.Two]

[satellites.RandomTransmitter.Sender]

[satellites.EudaqNativeWriter.Receiver]
_data_transmitters = ["Sender"]
output_directory = "/tmp/test"
```

```{important}
Make sure to create the output directory for the `EudaqNativeWriter`.
```

After the (failed) initialization, various log messages are shown. Log messages with log level `STATUS` are shown in green,
log messages with log level `WARNING` in orange and log message with log level `CRITICAL` in red. Additionally, in the bottom
right corner of the window a message counter is shown.

```{figure} observatory_init.png
Observatory main window after initialization
```

To further inspect a log messages, it is possible to double-click on them to open details about the log message.

```{figure} observatory_message_detail.png
:scale: 40 %
Log message details
```

To fix this issue, the configuration file has to be adjusted:

```toml
[satellites.Sputnik.One]

[satellites.Sputnik.Two]

[satellites.RandomTransmitter.Sender]

[satellites.EudaqNativeWriter.Receiver]
_data_transmitters = ["RandomTransmitter.Sender"]
output_directory = "/tmp/test"
```

Received messages can be cleared with the {bdg-primary}`Clear Messages` button. This helps to review the logs after
initializing with the adjusted configuration.

## Changing the Subscription Level

Constellation groups log messages in different severity levels, ranging from "very low-level information" (`TRACE`) to
"system-critical failure messages" (`CRITICAL`). More detailed information can be found in the
[logging & verbosity chapter](../concepts/logging.md)
The level of log messages to be received by this logger can be adjusted with the {bdg-primary}`Global Level` selection box
in the subscription section on the right.
After adjusting the subscription, new log messages of selected level and all higher levels are received when generated e.g.
by launching the satellites.
For debugging purposes, it can be useful to increase the log level beyond the default log level:

```{figure} observatory_debug.png
Observatory main window with debug messages
```

```{important}
Only log messages with log level `DEBUG` emitted _after_ changing the subscription are visible. It is not possible to view
debug messages before that moment since they have not been sent by the satellites.
```

The log level selection offered directly next to the sender name allows to change the subscription level for the *type* topic
of that sender, for `Sputnik.One` this would be `SPUTNIK`. This is usually the log topic under which instrument code will log
information.

It is also possible to increase the verbosity for a specific log topic of a sender. For example, to see heartbeating in
action it is possible to increase the verbosity of the `LINK` topic to `TRACE` for the `Sputnik.One` sender. This is achieved
by clicking on the name of the sender in the subscription window on the right and the adjusting the log level for the topic.
A list of common log topics is provided in [the logging section](../concepts/logging.md#log-topics).

```{figure} observatory_extra_subscription.png
Observatory main window with trace messages for heartbeating
```

```{important}
The individual subscription only provide the possibility to set _extra_ subscriptions. Setting a higher level than the global
level does not result in filtering of messages for that specific topic.
```

## Filtering Messages

Observatory allows applying four filters on the messages that have already been received and are displayed in the log message
list of the interface:

- Filtering by log level
- Filtering by sender
- Filtering by log topic
- Filtering by text matching log messages

For example, to find the `LINK` messages referring to `RandomTransmitter.Sender`, messages can be filtered using text matching
the name of that satellite.

```{figure} observatory_text_filter.png
Observatory main window with message filtering by matching text
```

All filters can be reset by clicking the {bdg-primary}`Reset` button next to the text filter.

Filters can be applied simultaneously. For example, it is possible to filter for all log messages with level `WARNING` or
higher from `RandomTransmitter.Sender` with the log topic `FSM`.

```{figure} observatory_multi_filter.png
Observatory main window with message filtering by log level, sender and log topic
```
