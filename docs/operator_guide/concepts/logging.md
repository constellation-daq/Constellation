# Logging & Verbosity Levels

Constellation comes with a powerful asynchronous logging mechanism which can transmit log messages with different verbosity
levels over network to other nodes, log them locally to the command line or file, or both simultaneously. *Asynchronous* in
this context means that the code which emits the log message is not held up by the processing of the message and its
storage or transmission via network, but continues directly. This way, even many log messages of very high verbosity level
will not affect the performance of the code.

The verbosity can be set independently for logging to the command line and by every receiving node on the Constellation
network. Only messages with a subscriber will actually be transmitted over the wire. In addition, the logging can be limited
to individual topics.

```{seealso}
Details about how to implement logging can be found in the
[application development guide](../../application_development/functionality/logging.md).
```

## Verbosity Levels

The verbosity levels have been designed to identify mistakes and implementation errors as early as possible and to provide
the user with clear indications about the problem. The amount of feedback can be controlled using different verbosity levels
which are inclusive, i.e. verbose levels also include messages from all levels above.

The following log levels are supported:

* **`CRITICAL`**: Indicates a fatal error. For a satellite this log message typically entails an exception and a transition
into the ERROR state. These log messages require immediate attention by the user.

* **`STATUS`**: Important information about the status of the application. Is used for low-frequency messages of high
importance such as a change of state of the finite state machine.

* **`WARNING`**: Indicate conditions that should not occur normally and possibly lead to unexpected results. The application
will however continue running without problems after a warning. A warning could for example be issued to indicate that a
configuration parameter was not used and that the corresponding key might contain a typographic mistake.

* **`INFO`**: Information messages about regularly occurring events, intended for the user of the application.

* **`DEBUG`**: In-depth details about the progress of the application with details concerning data processing and communication
with attached hardware. This verbosity level potentially produces a large number of messages and is mostly serving the
application developers (i.e. the person implementing the specific satellite) to debug procedures.

* **`TRACE`**: Messages to trace the call stack or communication patterns across the framework or within a satellite. Log
messages on this level contain many technical details usually catering to developers of the framework, including code
location information of the log message.

```{note}
For normal operation of a Constellation framework it is recommended so subscribe to `WARNING` level and above.
Not subscribing to `WARNING`, `STATUS` or `CRITICAL` messages could lead to missing information on critical incidents, while
logging verbosity levels lower than `INFO` for an extended period of time over the network may have a considerable impact on
on the bandwidth available to e.g. transmit data.
```
