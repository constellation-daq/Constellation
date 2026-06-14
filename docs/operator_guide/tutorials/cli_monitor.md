# Using the Command-Line Monitor

The `Monitor` tool is a lightweight command-line listener component. It subscribes to log messages from all satellites in the
Constellation, optionally subscribes to telemetry data, and prints everything to the terminal. Provided an output path, it
writes all received metrics and log messages to CSV files on disk.

```{seealso}
The [Logging & Verbosity Levels](../concepts/logging.md) concept page describes log levels and log topics while telemetry
data and their reception is described on the [Telemetry](../concepts/telemetry.md).
```

## Starting the Monitor

The monitor is started with the `Monitor` command. The only required argument is the group name of the
Constellation to connect to:

```sh
Monitor -g edda
```

The monitor discovers active satellites automatically through the [network discovery](../concepts/discovery.md) mechanism and
subscribes to their log messages and telemetry metrics. After connecting, the monitor starts writing arriving messages to the
terminal:

```text
|2026-06-16 12:16:04| INFO     [Sputnik.One][FSM] Reacting to transition initialize
|2026-06-16 12:16:04| INFO     [Sputnik.Two][FSM] Reacting to transition initialize
|2026-06-16 12:16:04| STATUS   [Sputnik.One][FSM] New state: initializing
|2026-06-16 12:16:04| STATUS   [Sputnik.Two][FSM] New state: initializing
|2026-06-16 12:16:05| STATUS   [Sputnik.Two][TEMPERATURE] 6.022419639251089 degC
```

Each line shows the current time, the log level of the message, the sender name and log topic in brackets, followed by the
log message. When activated, received metrics appear as logs on `STATUS` level, noting the sender, metric name, value and unit.

The monitor is stopped by pressing {kbd}`Control-c`.

```{note}
The Command-Line Monitor is a standalone listener, not a satellite and therefore does not appear in any controller. It can be
started and stopped at any time without influencing the running Constellation.
```

## Controlling the Console Verbosity

By default, the monitor subscribes to log messages at `INFO` level and above. This means that only messages with the levels
`INFO`, `WARNING`, `STATUS` and `CRITICAL` are received and printed. The subscription level can be adjusted with the
`--level` argument, using any of the levels described in the [Logging & Verbosity Levels](../concepts/logging.md) concepts
section:

```sh
Monitor -g edda --level DEBUG
```

The `--level` argument controls both the console output and the actual subscription to log levels. Only messages that are at
or above the chosen level are transmitted by the satellites. Consequently, lowering the level causes more network traffic and
for extended monitoring sessions it is recommended to stay at `INFO`, `WARNING` or `STATUS` unless actively debugging.

```{note}
Like all Constellation listeners, the command-line monitor only receives messages emitted *after* it connects. Messages that
were sent before the monitor was started are not delivered retroactively.
```


## Subscribing to Telemetry

By default, Monitor does not subscribe to any metrics distributed via the [Telemetry](../concepts/telemetry.md) mechanism.
The telemetry reception can be enabled with the `--metrics` argument. Passing it with no names subscribes to all metrics from
every active satellite in the Constellation:

```sh
Monitor -g edda --metrics
```

The subscription can be limited to individual metrics by listing their names after `--metrics`:

```sh
Monitor -g edda --metrics TEMPERATURE VOLTAGE
```

Only metrics with matching names are subscribed to and transmitted over the network.

## Writing Messages to Disk

When passing the `-o`/`--output-path` argument, the command-line monitor will write all received data to files in the
given directory. The directory is created automatically if it does not exist:

```sh
Monitor -g edda -o /tmp/monitor_output
```

Log messages and telemetry data are written separately:

**A rotating log file** is used for all log messages. The file is created at `<output_path>/log.csv` and every log message
received from any satellite is appended as a single CSV row in the following format:

```csv
2026-06-16T13:12:56.242,STATUS,Sputnik.Two,FSM,"New state: RUN"
2026-06-16T13:12:56.243,INFO,Sputnik.Two,FSM,"Calling running function of satellite..."
2026-06-16T13:12:57.603,INFO,MissionControl.zeus,OP,"Sending transition command `stop`"
2026-06-16T13:12:57.604,INFO,Sputnik.Two,FSM,"Reacting to transition stop"
```

The columns are the ISO8601-formatted timestamp, the log level, the canonical name of the sender, the log topic, and the
message text enclosed in double quotes. Double-quote characters within a message as well as newline characters are escaped as
doubled (`""`) and newline sequences `\n`, respectively.

**Per-metric CSV files** are created for every sender and every metric received. The files are named
`<output_path>/<sender>.<metric>.csv`, where `<metric>` is the lower-case name of the corresponding metric, and each received
value is appended as a new CSV row with the following format:

```csv
1781436058.856382, -49.88287555005745, 'degC'
1781436061.873698, -49.99978310615272, 'degC'
```

The columns are the Unix timestamp, with sub-second precision, the numeric value of the metric, and the metric unit enclosed
in single quotes. No header row is written to the CSV file, the metric name and sender are encoded in the filename.
For the `Sputnik.One` satellite emitting a `TEMPERATURE` metric, the file would be named `Sputnik.One.temperature.csv`.

## Configuring Log File Rotation

The log file rotates automatically once it reaches a size of 10 MB, keeping up to 10 backup files by default. The number of
backup files can be changed by providing the `--backup-count` parameter to the command-line monitor:

```sh
Monitor -g edda -o /tmp/monitor_output --backup-count 5
```

When the active `log.csv` reaches 10 MB, it is renamed to `log.csv.1`, any existing `log.csv.1` becomes `log.csv.2`, and so
on up to `log.csv.<backup-count>`. The oldest file is deleted when the limit is reached.

The rotation can be disabled by setting the backup count to zero. In this case, no rollover happens and the log file grows
infinitely.

## Controlling the File Log Level

The command-line monitor allows setting the level for writing log messages to file separately from the level for terminal
output. The file verbosity can be set using `--file-level`. This makes it for example possible to print only warnings to the
terminal while archiving a more detailed record to disk:

```sh
Monitor -g edda --level WARNING --file-level INFO -o /tmp/monitor_output
```

When `--output-path` is provided, the monitor automatically subscribes to log messages at the *lower one* of `--level`
and `--file-level`, so that both the console and the file receive messages at their configured verbosity. In the example
above, the monitor subscribes at `INFO` but prints only `WARNING` and above to the terminal, while writing everything from
`INFO` level upward to the log file.

The `--file-level` argument accepts the same level names as `--level`, and by default it is set to the same level as the
terminal output.
