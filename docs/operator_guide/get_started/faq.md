# Frequently Asked Questions

This is a list of frequently asked questions and commonly encountered issues when setting up and operating a Constellation.

## Starting Constellation Nodes

:::{dropdown} Satellites are not discovered / do not show up in controller
When satellites are not discovered or do not show up in controller interfaces such as MissionControl, the reason likely is an
issue in the network communication:

* The machines are not on the same subnet. The `ping` command can be used to test connection between machines.
* The satellite is in a different Constellation group. Groups are explained in the [Systems Architecture section](../concepts/constellation.md#network-discovery).
* A firewall is blocking incoming UDP or TCP packets. The [Configuring Firewalls](../howtos/firewalls.md) How-To Guide describes in detail which settings of the firewall need to be adjusted.
* The communication is routed over the wrong network interface. When starting Constellation nodes, the `-i`/`--interface` command line argument can be used to restrict communication to one or several network interfaces. Command line arguments are documented in the [Satellite section](../concepts/satellite.md#the-satellite-executable).
:::

:::{dropdown} Satellites on macOS cannot access the network
:::

## Configuration

:::{dropdown} MissionControl warns about satellites not found in configuration file
MissionControl issues a warning when it discovers satellites in the Constellation group that have no corresponding entry in
the loaded configuration file. This is a guard against typos: a misspelled satellite name would otherwise silently receive
an empty configuration.

If the satellite in question does indeed not need any configuration parameters, this warning can be safely ignored. The
message can be silenced by adding an explicit empty section for it in the configuration file:

```toml
[Sputnik.Three]
```

If the satellite was not expected to appear, it should be checked that its Constellation group name matches and that it is
not a leftover process from a previous session or supposed to be part of a different Constellation.
:::

:::{dropdown} An environment variable placeholder is not resolved or causes an error
If Constellation reports that an environment variable cannot be found, or the placeholder appears
literally in the final value, the following items should be checked:

* **Controller-side vs. satellite-side syntax.** Variables written as `${VARIABLE}` are resolved on the satellite host at the
  time the configuration key is first accessed by the satellite. Variables written as `$_{VARIABLE}` are resolved by the
  controller before the configuration is sent to individual satellites, and has to be present on the controller machine.
* **The variable is not set in the environment.** It should be ensured, e.g. by running `echo $VARIABLE` in the same shell as
  the node in question, that the variable in question is exported to the environment. Shell variables defined without the
  `export` keyword are not visible to child processes.
* **A default fallback can be specified** using the `:-` syntax: `${VARIABLE:-fallback}`. This prevents errors for absent
  variables and is especially useful during initial setup of a Constellation.

The [Configuration Files](../concepts/configuration_files.md#environment-variables) concept section describes the full
available syntax for parsing environment variables.
:::
