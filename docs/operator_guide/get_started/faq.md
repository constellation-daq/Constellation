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

If the satellite was not expected to appear, check that its Constellation group name matches and that it is not a leftover
process from a previous session or supposed to be part of a different Constellation.
:::
