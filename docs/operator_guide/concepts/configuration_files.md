# Configuration Files

Constellation uses configuration files as source for the parameters.
Controllers parse and validate these configuration files and distribute the derived configuration sections to the individual satellites for their {bdg-secondary}`initializing` state, as well as additional configuration parameter for the optional {bdg-secondary}`reconfiguring` state described in the [satellite section](./satellite.md#changing-states---transitions).

```{hint}
It is strongly recommended to keep configuration files under version control, for example by checking them into a `git` repository specifically designated for the configurations of the Constellation setup.
This allows configuration changes to be tracked and tried-and-tested configuration files to be used in further measurement campaigns.
```

This section describes the supported syntax and structure of configuration files and details some additional features such as default parameter values and environment variables.

## Supported File Syntax

Configuration files can be provided in either [YAML 1.2](https://yaml.org/spec/1.2.2/) or [TOML 1.0](https://toml.io/) syntax.
The versions have advantages and disadvantages in terms of readability and overview.

**YAML** uses indentation to structure the configuration.
While this may be tedious and error-prone when typing, it provides a good overview of the configuration structure at a glance.
Keys and values are separated by a colon.

**TOML** uses square brackets to denote sections, indentation is ignored, and values are assigned to keys with an equal sign.
This may be easier to read with a few keys, but additional subsections can quickly obscure the actual structure.

A comparison of a short configuration snippet in either syntax is provided below:

::::{grid} 1 1 2 2

:::{grid-item-card}
YAML syntax example
^^^^^^^^^^^^

```yaml
Sputnik:
  One:
    interval: 1500
  Two:
    interval: 500
```

:::

:::{grid-item-card}
TOML syntax example
^^^^^^^^^^^^


```toml
[Sputnik.One]
interval = 1500

[Sputnik.Two]
interval = 500
```

:::
::::

There are multiple online tools available which convert TOML into YAML and vice versa.

## File Structure

Configuration files are hierarchically structured into three main levels by satellite type, satellite instance, and instance parameters:

* The top-level keys identify satellite types such as `Sputnik`, `Mariner`, or `FlightRecorder`.
* Keys on the second level, immediately under the satellite type, represent individual, named instances of that type.
* Keys on the third level and below are parameter keys that define the specific operational settings of the respective satellite instance.

The combination of satellite type and instance name forms the [canonical name](./satellite.md#type-and-name) of the respective satellite, which has to be unique across the Constellation.
Some controllers issue a warning when satellites are found in the Constellation that do not have a corresponding entry in the loaded configuration file.
The following is an example with two Sputnik-type satellites and one Mariner, neither of them specifying parameters to these instances.

::::{tab-set-code}

```yaml
Sputnik:
  One:
  Two:
Mariner:
  Nine:
```

```toml
[Sputnik.One]
[Sputnik.Two]
[Mariner.Nine]
```

::::

## Parameter Keys & Values

Constellation configuration files support key-value pairs, where the key is always a string, and the values can be scalars, arrays or sections, which again consist of key-value pairs.
Configuration **keys are always interpreted as strings**, while the following **scalar value types** are distinguished:

* Boolean, with values `true` and `false`.
* Integer values such as `123`, `0x33`, or `0b11`. Integers can be provided in decimal (plain number), hexadecimal (with `0x` prefix) or binary notation (`0b` prefix).
* Floating point numbers, indicated by a decimal point or an exponent, such as `1.23` or `1e13`.
* Timestamps such as dates, daytime, or full timestamps, such as `2025-11-13`, `05:55:23`, or `2025-05-27T00:32:00-07:00`.
* Strings of text, enclosed in double-quotes such as `"configuration parameter with spaces"`.

In a configuration file, these types appear as in the following example.

::::{tab-set-code}

```yaml
Sputnik:
  One:
    bool: true
    integer: 123
    float: 1.23
    daytime: 05:55:23
    string: "configuration parameter with spaces"
```

```toml
[Sputnik.One]
bool = true
integer = 123
float = 1.23
daytime = 05:55:23
string = "configuration parameter with spaces"
```

::::

:::{dropdown} Technical note on YAML parsing
:icon: gear
:color: secondary

The configuration used in Constellation requires strong typing, this means that each value has a determined variable type such as *floating point*, *integer* or *string*. While TOML has this sort of typing defined in its syntax, YAML does not distinguish between different types and treats all scalar nodes as opaque data.

Hence, when parsing YAML it is upon the parser to determine and assign types. Those parsers attempt to convert each value to a boolean, integer, floating point number, and timestamp. If all conversions fail, the content will be interpreted as string.
:::

### Arrays

**Arrays** are lists of scalar values and can be written in a short or a long syntax format, also called *flow syntax* and *block syntax* for YAML.

```{note}
Although the file syntax allows mixing of types, configuration arrays in Constellation must be homogeneous, i.e., all elements must have the same scalar type.

```

::::{tab-set-code}

```yaml
Sputnik:
  One:
    array_block:
      - 1.3
      - 0.5
      - 1e15
    array_flow: [1.3, 0.5, 1e15]
```

```toml
[Sputnik.One]
array_block = [
  1.3,
  0.5,
  1e15
]
array_flow = [1.3, 0.5, 1e15]
```

::::

### Sections

Configuration **Sections**, also known as *tables* in TOML or *mappings* in YAML, allow to combine multiple key-value pairs under a common variable name.
Value types can be mixed freely, and sections can be recursively nested.
This means that the value of a section entry can be again either a scalar, an array, or a section itself.
Some satellites use this to better structure their configuration:

::::{tab-set-code}

```yaml
Sputnik:
  One:
    section_one:
      parameter_a: 12
      parameter_b: "access_token"
    section_other:
      channel: 5
      output: 1.3
```

```toml
[Sputnik.One]

[Sputnik.One.section_one]
parameter_a = 12
parameter_b = "access_token"

[Sputnik.One.section_other]
channel = 5
output = 1.3
```

::::

:::::{dropdown} Note on tables in TOML
:icon: gear
:color: secondary

TOML has three ways to write tables: with table headers, as inline tables or by prefixing keys.
Functionally all three are identical, and any way can be picked depending on personal preference.

::::{tab-set}
:::{tab-item} Table headers

```toml
[Sputnik._default.section_one]
parameter_a = 13

[Sputnik.One.section_one]
parameter_a = 12
parameter_b = "access_token"

[Sputnik.One.section_other]
channel = 5
output = 1.3
```

:::
:::{tab-item} Inline tables

```toml
[Sputnik._default]
section_one = { parameter_a = 13 }

[Sputnik.One]
section_one = { parameter_a = 12, parameter_b = "access_token" }
section_other = { channel = 5, output = 1.3 }
```

:::
:::{tab-item} Prefixed keys

```toml
[Sputnik]
_default.section_one.parameter_a = 13

[Sputnik.One]
section_one.parameter_a = 12
section_one.parameter_b = "access_token"

section_other.channel = 5
section_other.output = 1.3
```

:::
::::

It should be noted though that once a table has been defined in any of the three syntactic versions, it cannot be re-defined and additional keys have to be added in the same schema.

:::::

### Framework Parameters

Constellation uses a leading underscore to distinguish parameters from satellite implementations of parameters that are directed at framework components, and to avoid naming conflicts between them.
The satellite documentation lists these parameters in separate sections.

In the following, the parameter `_autonomy.role` is a Constellation framework parameter, while `interval` is a parameter that is read and interpreted by the satellite implementation:

::::{tab-set-code}

```yaml
Sputnik:
  One:
    _autonomy:
      role: "ESSENTIAL"
    interval: 1500
```

```toml
[Sputnik.One]
_autonomy.role = "ESSENTIAL"
interval = 1500
```

::::


### Default Parameter Values

Sometimes it can be beneficial to specify a parameter for all satellites of a specific type, or even for all satellites in the Constellation instead of copying it into every satellite instance section of the configuration.
For this purpose, Constellation configuration files feature the `_default` mechanism.
A section named `_default` will not be interpreted as satellite type or instance, but will be used as the default set of parameters for the given configuration section.

A set of default values for all satellites in the Constellation has to be placed at the top level of the file:

::::{tab-set-code}

```yaml
_default:
  _autonomy:
    role: ESSENTIAL

Sputnik:
  One:

Mariner:
  Nine:
```

```toml
[_default]
_autonomy.role = "ESSENTIAL"

[Sputnik.One]

[Mariner.Nine]
```

::::

Here, the `_autonomy.role` parameter will be applied to both satellite instances, `Sputnik.One` and `Mariner.Nine`.
In contrast, when placing the `_default` section *within* a satellite type, only the respective satellites will inherit these parameters:

::::{tab-set-code}

```yaml
Sputnik:
  _default:
    _autonomy:
      role: ESSENTIAL
  One:

Mariner:
  Nine:
```

```toml
[Sputnik._default]
_autonomy.role = "ESSENTIAL"

[Sputnik.One]

[Mariner.Nine]
```

::::

Here, only the `Sputnik.One` satellite will receive the `_autonomy.role` parameter value `ESSENTIAL`, while `Mariner.Nine` will have the framework-default value.

Parameter set through this `_default` mechanism are overwritten by more specific configurations.
This means, global parameters are overwritten by satellite-type default parameters, and satellite-type defaults are replaced by individual satellite configurations:

::::{tab-set-code}

```yaml
_default:
  _autonomy:
    role: ESSENTIAL

Sputnik:
  _default:
    _autonomy:
      role: TRANSIENT
  One:
    _autonomy:
      role: NONE
  Two:

Mariner:
  Nine:
```

```toml
[_default]
_autonomy.role = "NONE"

[Sputnik._default]
_autonomy.role = "TRANSIENT"

[Sputnik.One]
_autonomy.role = "ESSENTIAL"

[Sputnik.Two]

[Mariner.Nine]
```

::::

Here, `Sputnik.One` will obtain the parameter value `ESSENTIAL` set directly in its instance configuration, `Sputnik.Two` will receive the satellite-type default value of `TRANSIENT` while `Mariner.Nine` falls back to the global default value of `NONE`.


## Environment Variables

For configuration parameters such as access keys or tokens it can be beneficial to not store them directly in configuration
files, which might be under version control or shared to and accessible by a wide range of people, but to read them from the
current environment. For this purpose, Constellation supports placeholders for environment variables that are evaluated
at run-time.

### Placeholders Syntax

The syntax of placeholders is based on the familiar format of environment variables in the shell, i.e. a variable name
prefixed with a dollar sign. Since environment variables are always string representations, the usage of placeholders is
reserved for string-type configuration keys.

A configuration value can contain multiple and different environment variable placeholders.
They are placed directly in the value of the respective configuration key, for example:

```toml
file_path = "/home/${USER}/${CNSTLN_LOGDIR}/logfile.txt"
```

### Place of Resolution

Environment variables can either be present on the machine where the configuration file is read and parsed by the controller,
or only on the node that runs the satellite requiring the parameter. Hence, both concepts are available as described in the
following.

#### Satellite-Side Variables

*Satellite-side variables* are denoted using the syntax `${VARIABLE}`. They will be resolved on the satellite side, i.e.
only after the satellite has received the configuration and when it accesses the respective configuration key. The
resolution is performed on the node the satellite runs on. Substituted satellite-side variables will not be added to the
configuration and are therefore not accessible outside the satellite node, neither through the `get_config` command nor
through the run metadata. They are therefore ideal for storing secrets in satellite configurations that should not be
accessible to everyone.

A typical usage of a satellite-side environment variable is a secret not to be shared in the Constellation, such as the
access token portion of a [Mattermost web hook](../howtos/setup_mattermost_logger.md). On the node running the `Mattermost`
satellite, an environment variable is defined:

```sh
export MM_HOOK_TOKEN="9om7nhes7p859e1qrxi5dgykzr"
```

In the Constellation configuration file, this environment variable is now referenced:

```toml
[Mattermost.Logger]
webhook_url = "https://yourmattermost.com/hooks/${MM_HOOK_TOKEN}"
```

During the {bdg-secondary}`initializing` stage, the `Mattermost` satellite will access the `webhook_url` key. At this
moment, all environment variable placeholders in the value are resolved, and the `${MM_HOOK_TOKEN}` environment variable
will be substituted, resulting in the final value provided to the satellite being e.g.
`https://yourmattermost.com/hooks/9om7nhes7p859e1qrxi5dgykzr`.

#### Controller-Side Variables

*Controller-side variables* can be placed in configuration files using the syntax `$_{VARIABLE}`. These placeholders will
be resolved on the controller side, i.e. before encoding and sending the configuration to satellites with the
`initialize` or `reconfigure` commands. The resolution is performed on the node the controller runs on. When parsing
configuration keys on the controller side, an error is displayed when a referenced environment variable cannot be found.

```{warning}
Controller-side environment variables will be substituted by the controller and subsequently sent to the respective
satellites in clear text. Also retrieving the configuration from a satellite will contain the substituted values. They
should therefore not be used for secrets.
```

### Default Values for Environment Variables

In some cases it can be helpful to be able to specify a default fallback value to be used if the environment variable is not
set on the target node. Constellation supports this using the typical shell-like syntax separating the variable name from the
default value with the token `:-`:

```toml
[FlightRecorder.Logger]
file_path = "/home/${USER}/${CNSTLN_LOGDIR:-logs}/logfile.txt"
```

In case the environment variable `CNSTLN_LOGDIR` is not defined, the parameter `file_path` resolves to
`/home/myuser/logs/logfile.txt` without producing an error.

The default syntax can be used both for controller-side and satellite-side environment variables.

### Escaping Environment Variables

It might be necessary to provide a satellite with a literal string containing the pattern matching an environment variable.
In this case, the corresponding sequence can be escaped by prepending a backslash character `\`. This means:

* Controllers will not attempt to replace an environment variable in `\$_{VARIABLE}` but pass on a literal `$_{VARIABLE}` to the satellites.
* Satellites will not attempt to replace an environment variable in `\${VARIABLE}` but provide the literal `${VARIABLE}` to the satellite code.

## Examples

The following comprehensive example demonstrates the configuration of two Satellite Types, `Sputnik` and `Mariner`, showing use of the `_default` mechanism.

::::{tab-set-code}

```yaml
# Configuration for Sputnik-type satellites
Sputnik:
  # Default parameters inherited by all Sputnik instances
  _default:
    # Framework parameter
    _autonomy:
      max_heartbeat_interval: 10
    # Satellite-specific parameter
    interval: 3000

  # Sputnik instance with canonical name "Sputnik.One"
  One:
    # This overrides the default, setting it to 1500ms for this instance only
    interval: 1500
    # The _autonomy.max_heartbeat_interval is inherited as 10 seconds

  # Another instance, called "Sputnik.Two"
  Two:
    # Explicitly configuring a satellite-specific parameter:
    launch_delay: 5
    # Both interval (3000ms) and _autonomy.max_heartbeat_interval (10s) are
    # inherited from the _default section

# Configuration for Mariner-type satellites
Mariner:
  # Here, no _default section is included, and no parameters  are inherited

  # A Mariner-type satellite with name "Mariner.Nine"
  Nine:
    # Mariner-specific parameters
    voltage: 5.5
```

```toml
# Defining the top-level satellite type is optional in the TOML syntax
[Sputnik]

# Default parameters inherited by all Sputnik instances
[Sputnik._default]
# Framework parameter
_autonomy.max_heartbeat_interval = 10
# Satellite-specific parameter
interval = 3000

# Sputnik instance with canonical name "Sputnik.One"
[Sputnik.One]
# This overrides the default, setting it to 1500ms for this instance only
interval = 1500
# The _heartbeat_interval is inherited as 10 seconds

# Another instance, called "Sputnik.Two"
[Sputnik.Two]
# Explicitly configuring a satellite-specific parameter:
launch_delay = 5
# Both interval (3000ms) and _heartbeat_interval (10s)
# are inherited from the _default section

# Configuration for Mariner-type satellites
[Mariner]
# Here, no _default section is included, and no parameters  are inherited

# A Mariner-type satellite with name "Mariner.Nine"
[Mariner.Nine]
# Mariner-specific parameters
voltage = 5.5
```

::::
