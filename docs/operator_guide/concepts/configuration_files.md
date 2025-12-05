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

### Framework Parameters

### Default Values

### Environment Variables


The tutorial section holds a detailed [tutorial on including environment variables](../tutorials/env_variables.md) in configuration files.

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
