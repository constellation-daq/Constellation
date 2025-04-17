# Setting up a Mattermost Logger

Keeping a logs of measurements for later inspection is good practice for any experiment. However, logging often takes place locally in the lab and is not easily accessible during remote operation. Logging to a [Mattermost](https://mattermost.com/) channel allows to get notified anywhere about the important aspects such as interruptions and failures.

## Creating a Webhook

First, a channel in which logging should take place has to be selected or created. Then the integrations panel needs to be
opened by clicking the button in the top left from the Mattermost team which contains the channel.

```{figure} mattermost_open_integrations.png
Mattermost main window
```

On the integrations page, incoming webhooks has to be selected, and then "Add Incoming Webhook" in the top right.

```{figure} mattermost_integrations.png
Mattermost integrations window
```

In the creation screen a name for the webhook and a username have to be given. The username is the actual name displayed in
the channel. For use with Constellation, a channel for the webhook has to be selected. It is also possible to add a profile
picture for the integration. For the Constellation logo
`https://gitlab.desy.de/constellation/constellation/-/raw/main/docs/logo/logo_small.png` can be used.

```{figure} mattermost_new_incoming_webhook.png
Mattermost new incoming webhook window
```

Once saved, the webhook URL is shown. This URL needs to be used in the configuration of the `Mattermost` satellite.

## Connecting the Logger

To use the logger an installation with the [`Mattermost` satellite](../../satellites/Mattermost.md) is required.

```{note}
The `Mattermost` satellite is not built by default when building from source.
```

The `Mattermost` satellite can be started with:

```sh
SatelliteMattermost -g edda -n Logger
```

The satellite requires a configuration, which in the simplest case might look like this:

```toml
[satellites.Mattermost.Logger]
webhook_url = "https://yourmattermost.com/hooks/yourwebhook"
```

Assuming the configuration is stored in `config.toml`, the CLI controller can be started with:

```sh
Controller -g edda -c config.toml
```

The satellites can be initialized with the configuration using:

```python
constellation.initialize(cfg)
```

Now a log message should appear in the channel for which the webhook was created:

```{figure} mattermost_logger_connected.png
Mattermost channel with logger connected
```

In the default configuration, messages with log level `WARNING`, `STATUS` and `CRITICAL` are logged, the start and end of a
run and when an interruption is triggered. To avoid clutter from state changes, the logger ignores log messages with the
topic `FSM` by default. In practice a log might look like this:

```{figure} mattermost_demo_logs.png
Mattermost channel with some log messages
```

## Verbosity Configuration

If a higher verbosity is desired, the `log_level` parameter can be adjusted accordingly.

```{seealso}
Details about the available log levels can be found in the [concepts section](../concepts/logging.md).
```

Note that in many cases such as state changes, log messages are emitted from each satellite, resulting a plethora of messages
with the same content. This is the reason why the `FSM` topic is ignored by default. The set of ignored topics can be changed
with the `ignore_topics` parameter.

A log with `INFO` log level and no ignored topics might look like this:

```{figure} mattermost_high_verbosity.png
Mattermost channel with high verbosity logging
```

The small info icon next to the message time can be used to inspect the log level and log topic.
