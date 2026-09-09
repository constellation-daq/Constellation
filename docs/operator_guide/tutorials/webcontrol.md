# Using WebControl

WebControl is a browser-based controller for Constellation. This tutorial demonstrates how to use WebControl to
control multiple satellites, view logs and metrics, and send individual commands — all from a web browser.

```{seealso}
It is recommended to read through the tutorial on how to [start and control a single satellite](single_satellite.md) first.
WebControl provides the same control capabilities as [MissionControl](missioncontrol.md) through a web interface rather
than a native Qt application.
```

## Installation

WebControl is installed as a separate Python package that depends on Constellation:

```bash
pip install constellation-webcontrol
```

```{note}
The WebControl package includes a pre-built browser frontend. No Node.js installation is required for normal use.
```

## Starting WebControl

WebControl is started using the `WebControl` command. The group name of the Constellation to control must be provided
with the `-g` flag:

```bash
WebControl -g MyExperiment
```

This starts two servers:

- A **WebSocket server** on port 8765 (for real-time communication with the browser)
- An **HTTP server** on port 8080 (serving the browser interface)

Open `http://localhost:8080` in any modern browser to access the interface.

```{hint}
The ports can be changed with the `--port` and `--ui-port` flags. For example, to use port 9000 for the WebSocket
server and port 3000 for the UI:

    WebControl -g MyExperiment --port 9000 --ui-port 3000

Set `--ui-port 0` to disable the built-in HTTP server, for example when running the SvelteKit development server
during frontend development.
```

The WebControl interface can be divided into several areas:

- A **header bar** showing the group name, connection status, satellite count, and global state
- A **control panel** for sending commands to all satellites at once
- A **satellite list** showing each satellite's state, with individual control buttons
- A **log viewer** for monitoring system messages
- A **config picker** for selecting configuration files
- A **Command Station** for sending arbitrary commands to individual satellites

```{note}
If no WebSocket server is reachable, WebControl automatically enters a **simulation mode** that demonstrates the
interface using mock satellites and state transitions. This is useful for exploring the interface without running
any satellites.
```

## Initializing the Constellation

In order to control satellites, they need to be started as part of the same group. In this tutorial, three `Sputnik`
satellites named `One`, `Two` and `Three` are used. After starting, the satellites appear in the interface in the
`NEW` state.

```{note}
WebControl does not start satellites. The satellites required for this tutorial need to be started in a terminal as
shown in the [tutorial for a single satellite](single_satellite.md).
```

### Selecting a Configuration File

Before initializing, a configuration file can be selected using the config picker. Click the {bdg-primary}`Config` button in the
control panel to open the file picker, which lists all `.toml`, `.yaml`, and `.yml` files found in the working
directory (or the directory specified with `--config-dir`).

The following configuration file is used for this tutorial:

```toml
[Sputnik._default]
interval = 3000

[Sputnik.One]
interval = 2500

[Sputnik.Two]

[Sputnik.Three]
```

```{hint}
The configuration directory is set when starting WebControl. To point it at a specific directory:

    WebControl -g MyExperiment --config-dir /path/to/configs
```

### Sending the Initialize Command

After selecting the configuration file, click the {bdg-primary}`Initialize` button in the control panel to initialize all
satellites. The button is available whenever the global state allows initialization (when satellites are in {bdg-secondary}`NEW`,
{bdg-secondary}`SAFE`, or {bdg-secondary}`ERROR` states).

During initialization, satellites briefly enter the `initializing` transitional state, shown with a pulsing animation
in the interface. Once complete, all satellites reach the {bdg-secondary}`INIT` state.

The header bar updates to reflect the global state. If all satellites share the same state, it is displayed directly.
If satellites are in different states, the header shows the lowest common state with a "mixed" indicator.

```{seealso}
More details on the Constellation finite state machine and its different states and transitions can be found in the
[satellite chapter](../concepts/satellite.md#the-finite-state-machine).
```

### Handling Initialization Errors

To demonstrate error handling, consider what happens if the configuration contains an invalid value. For example,
if `Sputnik.One` is given a non-numeric `interval`:

```toml
[Sputnik._default]
interval = 3000

[Sputnik.One]
interval = "fast"

[Sputnik.Two]

[Sputnik.Three]
```

The `Sputnik` satellite expects `interval` to be an integer (milliseconds between beeps). When `Sputnik.One`
attempts to parse `"fast"` as an integer during initialization, the framework catches the type mismatch and
transitions that satellite to {bdg-secondary}`ERROR`. The other two satellites initialize normally to
{bdg-secondary}`INIT`, since each satellite processes its configuration independently.

The log viewer shows the failure reason reported by the satellite:

```text
CRITICAL  Sputnik.One  Critical failure: Could not convert value of type `string` to type `uint64` for key `interval`
```

When a satellite enters {bdg-secondary}`ERROR`, its satellite card turns red and the header bar reflects the mixed
state. The satellite can be recovered by correcting the configuration file and clicking {bdg-primary}`Initialize`
again (either on the individual satellite card, or globally via the control panel). Satellites already in
{bdg-secondary}`INIT` will be re-initialized with the corrected configuration as well.

```{tip}
Instead of re-initializing all satellites, it is also possible to initialize only the satellite which went to
{bdg-secondary}`ERROR` state by using the {bdg-primary}`Initialize` button on its individual satellite card.
```

## Recording Data

Once all satellites are in {bdg-secondary}`INIT`, they can be launched to {bdg-secondary}`ORBIT` by clicking the {bdg-primary}`Launch` button. In this state,
the configuration is fully applied and satellites are ready for data taking.

### Starting a Run

Data taking is organized in runs. Click the {bdg-primary}`Start` button to begin a new run. All satellites enter the {bdg-secondary}`RUN` state.
The control panel shows the active run identifier.

```{important}
WebControl can be closed and reopened without interrupting data taking, since satellites operate autonomously. When
WebControl reconnects, it discovers the running satellites and their current states through CHIRP.
```

### Stopping and Landing

The run can be stopped by clicking the {bdg-primary}`Stop` button. Satellites return to {bdg-secondary}`ORBIT`, ready for another run.

To change the configuration or shut down satellites, click {bdg-primary}`Land` to return them to {bdg-secondary}`INIT`, or {bdg-primary}`Shutdown` to
terminate them.

## Controlling Individual Satellites

Each satellite card in the interface has its own set of command buttons that reflect the satellite's current state.
Only valid transitions are shown. For example, a satellite in {bdg-secondary}`ORBIT` shows {bdg-primary}`Start`, {bdg-primary}`Land`, {bdg-primary}`Reconfigure`,
and {bdg-primary}`Interrupt` buttons, while a satellite in {bdg-secondary}`RUN` shows {bdg-primary}`Stop` and {bdg-primary}`Interrupt`.

This allows controlling satellites independently. For example, one satellite can be stopped and re-initialized while
others continue running.

### The Command Station

For commands beyond the standard FSM transitions, each satellite card has a {bdg-primary}`Commands` button that opens the
Command Station. This panel shows all commands advertised by the satellite, organized into:

- **Standard getters** (`get_state`, `get_config`, `get_version`, etc.) shown without a payload field since they
  take no arguments
- **Custom commands** specific to the satellite type, shown with a JSON payload editor

Results are displayed inline, showing the response message, payload, and metadata. A session log at the bottom tracks
all commands sent to that satellite during the current session.

```{hint}
The Command Station only shows commands that are not already handled by the FSM buttons. Standard FSM commands
(`initialize`, `launch`, `start`, `stop`, `land`, `shutdown`, `reconfigure`) are always available through the
dedicated buttons.
```

## Viewing Logs

The log viewer at the bottom of the interface shows system messages from all satellites. Logs are received in real
time through CMDP subscriptions and can be filtered by:

- **Level**: `STATUS`, `INFO`, `WARNING`, `CRITICAL`, `DEBUG`, or `ALL`
- **Text search**: free-text filtering across sender, level, and message content
- **Sender**: per-satellite filtering via the subscription controls

The log viewer auto-scrolls to show the most recent messages, but scrolling up pauses auto-scroll to allow reviewing
older entries. The buffer is capped at 500 entries to maintain browser performance.

Warning and critical counts are shown per satellite in the satellite cards, providing a quick overview of which
satellites have reported issues.

## Viewing Metrics

Metrics reported by satellites appear in the metrics section. Each satellite card can show pinned metrics directly
on the card for quick reference.

To manage metric subscriptions, use the {bdg-primary}`Metrics` button to open the metrics picker. Metrics can be pinned to
satellite cards for at-a-glance monitoring, or unpinned to reduce visual clutter.

```{hint}
Metric subscriptions are managed per client. Pinning a metric tells the server to include it in the subscription,
and unpinning all metrics reverts to receiving all available metrics.
```

## Multiple Clients

WebControl supports multiple simultaneous browser clients. All clients receive state updates, log entries, and
metrics in real time. Commands sent from any client are executed on the same Constellation; there is no isolation
between clients.

Each client maintains its own log level filter, sender filter, and metric subscriptions independently.

## WebSocket Protocol Reference

WebControl communicates between the browser and the bridge server over a JSON-based WebSocket protocol. This
section documents the message types for developers building alternative clients or debugging connections.

### Client to Server

| Message type          | Fields                            | Description                                 |
| --------------------- | --------------------------------- | ------------------------------------------- |
| `command`             | `satellite`, `command`, `payload` | Send a CSCP command to a satellite          |
| `ping`                | (none)                                 | Heartbeat check; server replies with `pong` |
| `list_configs`        | (none)                                 | Request available configuration files       |
| `get_commands`        | `satellite`                       | Request advertised commands for a satellite |
| `subscribe_logs`      | `min_level`, `topics`, `senders`  | Adjust log subscription                     |
| `unsubscribe_logs`    | (none)                                 | Stop receiving logs                         |
| `subscribe_metrics`   | `names`, `senders`                | Adjust metric subscription                  |
| `unsubscribe_metrics` | (none)                                 | Stop receiving metrics                      |

### Server to Client

| Message type        | Fields                                                              | Description                                               |
| ------------------- | ------------------------------------------------------------------- | --------------------------------------------------------- |
| `state`             | `data.satellites`, `data.group`                                     | Full state snapshot (sent on connect and on every change) |
| `command_result`    | `success`, `message`, `payload`, `metadata`, `satellite`, `command` | Result of a command                                       |
| `log`               | `level`, `sender`, `message`, `timestamp`                           | Log entry                                                 |
| `metric`            | `sender`, `name`, `value`, `unit`, `description`, `timestamp`       | Metric update                                             |
| `commands`          | `satellite`, `commands`                                             | Advertised commands for a satellite                       |
| `config_list`       | `files`                                                             | Available configuration files                             |
| `config_list_error` | `message`                                                           | Error listing configuration files                         |
| `pong`              | (none)                                                                   | Reply to `ping`                                           |

```{note}
The `state` message is sent immediately upon connection, providing the client with the current state of all
satellites. Subsequent `state` messages are sent whenever a satellite's state changes or a satellite is
discovered or departs.
```

```{seealso}
The full WebSocket protocol reference with payload schemas and subscription architecture is available in the
[WebController repository](https://gitlab.desy.de/constellation/webcontroller/-/blob/main/docs/protocol.md).
A developer guide for extending the frontend and backend is also available in the
[same repository](https://gitlab.desy.de/constellation/webcontroller/-/blob/main/docs/developer_guide.md).
```
