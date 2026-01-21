# Configuration Sections

Configuration sections can be used to separate different functionality of a satellite into separate logical blocks, or to logically structure a complex set of parameter in a configuration file

```{seealso}
The basic concepts behind satellite configurations and configuration files in Constellation are described the [*Configuration Files* chapter of the operator guide](../../operator_guide/concepts/configuration_files.md).
```

A typical example where sections come in handy are satellites controlling instruments with multiple channels, such as power supply outputs.
Here, sections can be used to describe and configure each channel individually:

::::{tab-set-code}

```yaml
MyPSU:
  Bias:
    port: /dev/ttyUSB0
    channels:
      channel_0:
        enabled: true
        name: 5V Logic
        voltage: 5.0
      channel_1:
        enabled: true
        name: 3.3V Logic
        voltage: 3.3
```

```toml
[MyPSU.Bias]
port = "/dev/ttyUSB0"

[MyPSU.Bias.channels]

channel_0.enabled = true
channel_0.name = "5V Logic"
channel_0.voltage = 5

channel_1.enabled = true
channel_1.name = "3.3V Logic"
channel_1.voltage = 3.3
```

::::

This case with two fixed channels can be implemented in C++ or Python as demonstrated in the following.
The parameters not nested in a section are read directly from the `Configuration` objects, while the individual
channel configurations are retrieved via the `getSection()`/`get_section()` methods:

::::{tab-set}
:::{tab-item} C++
:sync: cxx

```cpp
// Get port for connection
const auto port = config.get<std::string>("port");

// Get configuration section for channels
const auto& channels_section = config.getSection("channels");

// Read channel 0 & 1 as nested configuration sections
for(auto n : {0, 1}) {

  // Get section for channel n
  const auto& channel_section = config.getSection("channel_" + to_string(n));

  // Note that channel_section has the same methods available as config

  // Get if channel is enabled, its assigned name and the set voltage
  const auto enabled = channel_section.get<bool>("enabled");
  const auto name = channel_section.get<std::string>("name");
  const auto voltage = channel_section.get<double>("voltage");
}
```

:::
:::{tab-item} Python
:sync: python

```python
# Get port for connection
port = config.get("port", return_type=str)

# Get configuration section for channels
channels_section = config.get_section("channels")

# Read channel 0 & 1 as nested sections
for n in [0, 1]:

    # Get section for channel n
    channel_section = config.get_section(f"channel_{n}");

    # Note that channel_section has the same methods available as config

    # Get if channel is enabled, its assigned name and the set voltage
    enabled  = channel_section.get("enabled", return_type=bool)
    name = channel_section.get("name", return_type=str)
    voltage = channel_section.get_num("voltage")
```

:::
::::

```{important}
Sections cannot be stored directly and used to access the contained parameters at a later stage since
all contained parameters which are not accessed with the framework-provided `get`-methods are removed
after the initializing and reconfiguring transitions.
```

## Default Sections

In the example above, both `channel_0` and `channel_1` were required to be explicitly defined in the configuration file.
This might be feasible when only a handful of channels are used, but for devices with many channels this approach can
quickly become cumbersome to type out if only a few of them are used.

It is therefore possible to define default values also for configuration sections in a similar way as for normal parameters.
Instead of specifying all nested default values when retrieving the section from the configuration, an empty placeholder `{}`
is added as parameter to the method fetching the section:

::::{tab-set}
:::{tab-item} C++
:sync: cxx

```cpp
// Get configuration section for channels
const auto& channels_section = config.getSection("channels", {});

// Read 128 channels as nested configuration sections
for(auto n : std::views::iota(0, 128)) {

  // Get section for channel n
  const auto& channel_section = config.getSection("channel_" + to_string(n), {});

  // Get if channel is enabled, its assigned name and the set voltage with defaults
  const auto enabled = channel_section.get<bool>("enabled", false);
  const auto name = channel_section.get<std::string>("name", "CHANNEL " + to_string(n));
  const auto voltage = channel_section.get<double>("voltage", 0.0);
}
```

:::
:::{tab-item} Python
:sync: python

```python
# Get configuration section for channels
channels_section = config.get_section("channels", {})

# Read 128 channels as nested configuration sections
for n in range(0, 128):

    # Get section for channel n
    channel_section = config.get_section(f"channel_{n}", {});

    # Get if channel is enabled, its assigned name and the set voltage with defaults
    enabled  = channel_section.get("enabled", False, return_type=bool)
    name = channel_section.get("name", f"CHANNEL {n}" return_type=str)
    voltage = channel_section.get_num("voltage", 0.0)
```

:::
::::

```{important}
If the satellite should be reconfigurable, all parameters should have a default value.
See [Reconfiguring](#reconfiguring) for more details.
```

In certain scenarios it might not be desirable that a default value is used when no explicit value is set in the configuration.
In the example above, assuming a voltage of `0.0` *should* be safe if the channel is enabled, but it would be better to notify the operator instead.
This can be done in the following way:

::::{tab-set}
:::{tab-item} C++
:sync: cxx

```cpp
const auto enabled = channel_section.get<bool>("enabled", false);
const auto name = channel_section.get<std::string>("name", "CHANNEL " + to_string(n));
if(enabled && !channel_section.has("voltage")) {
    // If enabled, require voltage explicitly to avoid default value
    throw MissingKeyError(channel_section, "voltage");
}
const auto voltage = channel_section.get<double>("voltage", 0.0);
```

:::
:::{tab-item} Python
:sync: python

```python
enabled  = channel_section.get("enabled", False, return_type=bool)
name = channel_section.get("name", f"CHANNEL {n}" return_type=str)
if enabled and "voltage" not in channel_section:
    # If enabled, require voltage explicitly to avoid default value
    raise MissingKeyError(channel_section, "voltage")
voltage = channel_section.get_num("voltage", 0.0)
```

:::
::::

## Dynamic Sections

It can be useful to define and use configuration sections with key names that are not fixed by the satellite code but dynamically defined by the user when writing the configuration file.
For example, a satellite which only logs messages for a configurable list of topics from a set satellites defined in the configuration file:

::::{tab-set-code}

```yaml
MyLogger:
  Logger:
    subscriptions:
      MyPSU.Bias:
        topics:
          MYPSU: DEBUG
      MyDet.DUT:
        topics:
          MYDET: DEBUG
          DATA: INFO
```

```toml
[MyLogger.Logger.subscriptions]
"MyPSU.Bias".topics = { MYPSU = "DEBUG" }
"MyDet.DUT".topics = { MYDET = "DEBUG", DATA = "INFO" }
```

::::

In this case, it is required to iterate over all keys that are defined for a specific section:

::::{tab-set}
:::{tab-item} C++
:sync: cxx

```cpp
// Get configuration section for subscriptions
const auto& subscriptions = config.getSection("subscriptions");

// Iterate over all keys in the subscriptions section
for(const auto& canonical_name : subscriptions.getKeys()) {

    // Get section corresponding to key (= canonical satellite name)
    const auto& satellite_section = subscriptions.getSection(canonical_name);

    // Get configuration section for topics
    const auto& topics = satellite_section.getSection("topics");

    // Iterate over all keys in the topics section
    for(const auto& topic : topics.getKeys()) {

        // Get log level for topic
        const auto log_level = topics.get<Level>(topic);
    }
}
```

:::
:::{tab-item} Python
:sync: python

```python
# Get configuration section for subscriptions
subscriptions = config.get_section("subscriptions")

# Iterate over all keys in the subscriptions section
for canonical_name in subscriptions.get_keys():

    # Get section corresponding to key (= canonical satellite name)
    satellite_section = subscriptions.get_section(canonical_name)

    # Get configuration section for topics
    topics = satellite_section.get_section("topics");

    # Iterate over all keys in the topics section
    for topic in topics.get_keys():

        # Get log level for topic
        log_level = topics.get(topic);
```

:::
::::

```{important}
With dynamic sections, the possibility to reconfigure is limited to existing parameters.
See [Reconfiguring](#reconfiguring) for more details.
```

## Reconfiguring

It is important to keep the assumptions Constellation makes in mind when implementing to reconfigure a satellite.
These assumptions are:

- Reconfiguring does not re-initialize the satellite, it only changes parameters
- Parameters which are changed have to exist already in the configuration
- Parameters cannot change their type
- Unused parameters (including nested sections) are removed from configuration

This means that [dynamic sections](#dynamic-sections) are severely limited in the way they can be reconfigured, as no new
sections can be defined and existing sections cannot be removed.
Whenever possible, default sections should be preferred over dynamic sections.

When default sections are used, it is important that all parameters within the section are fetched with a default value.
Otherwise it is not possible to reconfigure those parameters since they will not be present in the section.
