# Configuring Firewalls

On some operating systems, the pre-installed firewall blocks incoming network traffic on ephemeral ports in its default configuration.
In the following, several configuration possibilities are presented that allow nodes to communicate with the Constellation.
The preferred solution should be chosen according to the individual security requirements of the machine.

## Disabling the firewall

The most drastic approach to allow communication is to disable the firewall entirely.
While this is not generally recommended, it might still be a viable and easy solution given the [thread model](../concepts/constellation.md#threat-model-considerations) Constellation operates under.

For systems using `systemd` and `firewalld`, the service can be stopped, and optionally disabled permanently via:

```sh
systemctl stop firewalld
systemctl disable firewalld
```

## Accepts UDP packets on port 7123

The *CHIRP* protocol uses UDP port 7123, which needs to accept incoming packets for network discovery.
If Constellation nodes such as satellites are not found or do not appear in controller interfaces, the firewall of the
machine on which this happens might block the corresponding packets. Allowing them works via:

```sh
firewall-cmd --permanent --add-port=7123/udp
```

## Allowing incoming TCP traffic

An alternative approach is to accept all incoming TCP packets on ephemeral ports. For `firewalld` this can be achieved with:

```sh
firewall-cmd --add-port=32768-60999/tcp
firewall-cmd --reload
```

If this setting should be made permanent to also prevail after a reboot of the machine, the `--permanent` argument has to be passed to `firewall-cmd`.

## Allowing incoming TCP traffic from a subnet

In order to further reduce possible attack surfaces, incoming TCP packets can be accepted only from a specific subnet.
This is especially useful when all other Constellation nodes run in the same subnet and also exclusively communicate over it.
The following rule limits incoming TCP packets on ephemeral ports to the subnet `192.168.1.0/24`:

```sh
firewall-cmd --add-rich-rule='rule family="ipv4" source address="192.168.1.0/24" port port="32768-60999" protocol="tcp" accept'
firewall-cmd --reload
```

Again, making this setting permanent requires the additional the `--permanent` argument for `firewall-cmd`.

Alternatively, these settings can also be made for individual zones.
More information can be found in the [firewalld documentation](https://firewalld.org/documentation/zone/).
