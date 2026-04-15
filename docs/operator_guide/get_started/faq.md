# Frequently Asked Questions

This is a list of frequently asked questions and commonly encountered issues when setting up and operating a Constellation.

## Starting Constellation Nodes

:::{dropdown} Satellites are not discovered / do not show up in controller

When satellites are not discovered, do not show up in controller interfaces such as MissionControl, the reason likely
is an issue in the network communication:

* The machines are not on the same subnet. The `ping` command can be used to test connection between machines.
* The satellite is in a different Constellation group. Groups are explained in the [Systems Architecture section](../concepts/constellation.md#network-discovery).
* A firewall is blocking incoming UDP or TCP packets. The [Configuring Firewalls](../howtos/firewall.md) How-To Guide describes in detail which settings of the firewall need to be adjusted
* The communication is outed over the wrong network interface. When starting Constellation nodes, the `-i`/`--interface` command line argument can be used to restrict communication to one or several network interfaces. Command line arguments are documented in the [Satellite section](../concepts/satellite.md#the-satellite-executable).

:::

:::{dropdown} Satellites on macOS cannot access the network

:::
