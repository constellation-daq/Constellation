# Using Observatory

Observatory is a graphical listener for Constellation. This tutorial demonstrates how to use Observatory to listen to log
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

- A filter section on top
- A subscription section on the right
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

Existing messages can be cleared with the {bdg-primary}`Clear Messages` button. This helps to review the logs after
initializing with the adjusted configuration.

## Changing the Subscription Level

For debugging purposes, it can be useful to increase the log level beyond the default `INFO` log level.
The log level can be change by adjusting the {bdg-primary}`Global Level` combo box in the subscription section on the right.
After the adjusting the subscription, new log messages can be generated by launching the satellites.

```{figure} observatory_debug.png
Observatory main window with debug messages
```

```{important}
Only log messages with log level `DEBUG` emitted _after_ changing the subscription are visible. It is not possible to view
debug messages before that moment.
```

It is also possible to increase the verbosity for a specific log topic of a sender. For example, to see heartbeating in
action it is possible to increase the verbosity of the `CHP` topic to `TRACE` for the `Sputnik.One` sender. This is achieved
by clicking on the name of the sender in the subscription window on the right and the adjusting the log level for the topic.

```{figure} observatory_extra_subscription.png
Observatory main window with trace messages for heartbeating
```

```{important}
The individual subscription only provide the possibility to set _extra_ subscriptions. Setting a higher level than the global
level does not result in filtering of messages for that specific topic.
```

## Filtering Messages

Observatory allows applying four filters:

- Filtering by log level
- Filtering by sender
- Filtering by log topic
- Filtering by text matching log messages

For example, to find the `CHP` messages referring to `RandomTransmitter.Sender`, messages can be filtered using text matching
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
