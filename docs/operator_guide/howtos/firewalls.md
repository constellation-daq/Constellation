# Configuring Firewalls

On some operating systems, the pre-installed firewall blocks incoming network traffic on ephemeral ports in its default configuration.
In the following, several configuration possibilities are presented that allow nodes to communicate with the Constellation.
The preferred solution should be chosen according to the individual security requirements of the machine.

Depending on the operating system, different tools are used as firewall, `firewalld` can be found for example on AlmaLinux
and Fedora, `ufw` is used on Ubuntu, and macOS uses `pf`.

## Disabling the firewall

The most drastic approach to allow communication is to disable the firewall entirely.
While this is not generally recommended, it might still be a viable and easy solution given the [thread model](../concepts/constellation.md#threat-model-considerations) Constellation operates under.

The service can be stopped, and optionally disabled permanently via:

::::{tab-set}
:::{tab-item} `firewalld`
:sync: firewalld

```sh
systemctl stop firewalld
systemctl disable firewalld
```

:::
:::{tab-item} `ufw` (Ubuntu)
:sync: ufw

```sh
systemctl stop ufw
systemctl disable ufw
```

:::
:::{tab-item} `pf` (macOS)
:sync: macos

```sh
pfctl -d
```

:::
::::

## Accept UDP packets on port 7123

The *CHIRP* protocol uses UDP port 7123, which needs to accept incoming packets for network discovery.
If Constellation nodes such as satellites are not found or do not appear in controller interfaces, the firewall of the
machine on which this happens might block the corresponding packets. Allowing them works via:

::::{tab-set}
:::{tab-item} `firewalld`
:sync: firewalld

```sh
firewall-cmd --permanent --add-port=7123/udp
```

The changes take effect immediately.

:::
:::{tab-item} `ufw` (Ubuntu)
:sync: ufw

```sh
ufw allow 7213/udp
```

The changes take effect immediately.

:::
:::{tab-item} `pf` (macOS)
:sync: macos

The following line should be appended to `/etc/pf.conf`:

```text
pass in proto udp to any port 7213
```

Afterwards, the configuration must be reloaded and the firewall restarted for the new settings to take effect:

```sh
pfctl -f /etc/pf.conf
pfctl -e
```

:::
::::

## Allow incoming TCP traffic

An alternative approach is to accept all incoming TCP packets on ephemeral ports. This can be achieved with:

::::{tab-set}
:::{tab-item} `firewalld`
:sync: firewalld

```sh
firewall-cmd --add-port=32768-65535/tcp
firewall-cmd --reload
```

If this setting should be made permanent to also prevail after a reboot of the machine, the `--permanent` argument has to be passed to `firewall-cmd`.

:::
:::{tab-item} `ufw` (Ubuntu)
:sync: ufw

```sh
ufw allow 32768:65535/tcp
```

The changes take effect immediately.

:::
:::{tab-item} `pf` (macOS)
:sync: macos

The following line should be appended to `/etc/pf.conf`:

```text
pass in proto tcp to any port 32768:65535
```

The firewall requires a restart.

:::
::::

## Allow incoming TCP traffic from subnet

In order to further reduce possible attack surfaces, incoming TCP packets can be accepted only from a specific subnet.
This is especially useful when all other Constellation nodes run in the same subnet and also exclusively communicate over it.
The following rule limits incoming TCP packets on ephemeral ports to the subnet `192.168.1.0/24`:

::::{tab-set}
:::{tab-item} `firewalld`
:sync: firewalld

```sh
firewall-cmd --add-rich-rule='rule family="ipv4" source address="192.168.1.0/24" port port="32768-65535" protocol="tcp" accept'
firewall-cmd --reload
```

Again, making this setting permanent requires the additional the `--permanent` argument for `firewall-cmd`.

Alternatively, these settings can also be made for individual zones.
More information can be found in the [firewalld documentation](https://firewalld.org/documentation/zone/).

:::
:::{tab-item} `ufw` (Ubuntu)
:sync: ufw

```sh
ufw allow from 192.168.1.0/24 to any port 32768:65535 proto tcp
```

The changes take effect immediately.

:::
:::{tab-item} `pf` (macOS)
:sync: macos

The following line should be appended to `/etc/pf.conf`:

```text
pass in proto tcp from 192.168.1.0/24 to any port 32768:65535
```

The firewall requires a restart.

:::
::::
