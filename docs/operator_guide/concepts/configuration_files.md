# Configuration Files

## Supported Syntax

The configuration file parsers of the available controller interfaces can parse files with either [YAML 1.2](https://yaml.org/spec/1.2.2/) or [TOML 1.0](https://toml.io/) syntax.
The versions have advantages and disadvantages in terms of readability and overview.

YAML uses indentation to structure sections of the configuration.
While this may be tedious and error-prone when typing, it provides a good overview of the configuration structure at a glance.
Keys and values are separated by a colon.

TOML uses square brackets to denote sections, indentation is ignored, and values are assigned to keys with an equal sign.
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

## Parameter Keys & Values

:::{dropdown} Technical note on YAML parsing
:icon: gear
:color: secondary

The configuration used in Constellation requires strong typing, this means that each value has a determined variable type such as *floating point*, *integer* or *string*. While TOML has this sort of typing defined in its syntax, YAML does not distinguish between different types and treats all scalar nodes as opaque data.

Hence, when parsing YAML it is upon the parser to determine and assign types. Constellation uses the [yaml-cpp](https://github.com/jbeder/yaml-cpp) library and its
`convert` methods to obtain typed values from YAML scalars. First, a conversion to a Boolean is attempted, then to an Integer, Floating point number, and timestamp, respectively. If all conversions fail, the content will be interpreted as string.
:::

### Framework Parameters

### Default Values

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

#### Controller-Side Variables

*Controller-side variables* can be placed in configuration files using the syntax `_${VARIABLE}`. These placeholders will
be resolved on the controller side, i.e. before encoding and sending the configuration to satellites with the
`initialize` or `reconfigure` commands. The resolution is performed on the node the controller runs on. When parsing
configuration keys on the controller side, an error is displayed when a referenced environment variable cannot be found.

```{warning}
Controller-side environment variables will be substituted by the controller and subsequently sent to the respective
satellites in clear text. Also retrieving the configuration from a satellite will contain the substituted values. They
should therefore not be used for secrets.
```

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


### Default Values for Environment Variables

In some cases it can be helpful to be able to specify a default fallback value to be used if the environment variable is not
set on the target node. Constellation supports this using the typical shell-like syntax separating the variable name from the
default value with the token `:-`:

```toml
[FlightRecorder.Logger]
file_path = "/home/${USER}/${CNSTLN_LOGDIR:-logs}/logfile.txt"
```

In case the environment variable `CNSTLN_LOGGER` is not defined, the parameter `file_path` resolves to
`/home/myuser/logs/logfile.txt` without producing an error.

The default syntax can be used both for controller-side and satellite-side environment variables.

## Examples

The following comprehensive example demonstrates the configuration of two Satellite Types, `Sputnik` and `Mariner`, showing use of the `_default` mechanism.

::::{tab-set-code}

```{code-block} yaml
# Configuration for Sputnik-type satellites
Sputnik:
  # Default parameters inherited by all Sputnik instances
  _default:
    # Framework parameter
    _heartbeat_interval: 10
    # Satellite-specific parameter
    interval: 3000

  # Sputnik instance with canonical name "Sputnik.One"
  One:
    # This overrides the default, setting it to 1500ms for this instance only
    interval: 1500
    # The _heartbeat_interval is inherited as 10 seconds

  # Another instance, called "Sputnik.Two"
  Two:
    # Explicitly configuring a satellite-specific parameter:
    launch_delay: 5
    # Both interval (3000ms) and _heartbeat_interval (10s) are
    # inherited from the _default section

# Configuration for Mariner-type satellites
Mariner:
  # Here, no _default section is included, and no parameters  are inherited

  # A Mariner-type satellite with name "Mariner.Nine"
  Nine:
    # Mariner-specific parameters
    voltage: 5.5
```

```{code-block} toml
# Defining the top-level satellite type is optional in the TOML syntax
[Sputnik]

# Default parameters inherited by all Sputnik instances
[Sputnik._default]
# Framework parameter
_heartbeat_interval = 10
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
