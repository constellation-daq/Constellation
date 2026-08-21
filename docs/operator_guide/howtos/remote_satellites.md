# Starting Remote Satellites

Commonly, not all or even no satellites run on the machines directly accessible to the operator.
A common task therefore is the orchestrated start of several satellites on remote machines. Depending on the use case, the
longevity of the experimental setup and the corresponding resources in setting up the Constellation, there are various
possible solutions, some of which are outlined below.

## Using Detached Processes

In general, it is not recommended to use `ssh` to log into a remote machine and to start satellites directly. Doing so
results in the stability of the Constellation satellite depending on the `ssh` session and connection. If for any reason
the connection drops, the satellite process is killed by the remote system.

It is therefore **strongly advised** to use detached processes on remote machines, i.e. processes whose lifetime does not depend
on an active login session. This can be achieved in different ways:

* Terminal emulators such as `tmux`, as described in the [Terminal emulator section](#starting-satellites-with-terminal-emulators)
  below, are the recommended procedure for small setups and quick turnaround times.
* Infrastructure provisioning tools, such as *Ansible* described in the [Ansible section](#infrastructure-provisioning-with-ansible),
  provide a more flexible alternative but require additional configuration on the operator side and are recommended for more
  permanent Constellation deployments or large setups with many satellites.
* Other solutions to run processes in the background such as `nohup`, `python-daemon` or `daemonize` can also be used but
  offer little advantage over terminal emulators.

## Starting Satellites with Terminal Emulators

If the remote hosts on which satellites should be started can be reached via `ssh` and has either `tmux` or GNU `screen`
available, satellites can be directly started with a single command.

With the **`tmux` terminal multiplexer**, a *Sputnik*-type satellite named *One* can be started in Constellation *edda* on the remote host using the
following command:

```sh
ssh -t user@remote tmux new -ds sputnik /path/to/SatelliteSputnik -g edda -n One
```

Here, the `-d` option will tell `tmux` to not attach the new session to the active terminal, and `ssh` will close the
connection immediately after executing the command. The `-s` switch is used to name the session. This name can be used at a
later point to reconnect and attach to this specific session using the command:

```sh
ssh -t user@remote tmux attach -t sputnik
```

If it is preferred to keep a terminal window with the satellite output open, the `-d` switch can be omitted. The `tmux`
session will then remain open. It can be detached using {kbd}`ctrl-b d`. Other `tmux` features are well described in the
[`tmux` Cheatsheet](https://tmuxcheatsheet.com/).

Similarly, **GNU `screen`** can be used for remote sessions. The same satellite setup as above can be started with the following
command:

```sh
ssh user@remote screen -S sputnik -d -m /path/to/SatelliteSputnik -g edda -n One
```

Also here, the `-d` parameter will detach the session directly and an omission thereof will keep the session attached.


## Infrastructure Provisioning with Ansible

more complex setups, takes a bit of time to set up but is flexible

[Ansible](https://docs.ansible.com/)

use in conjunction with [`systemd` services](./setup_systemd.md)

### Inventory

### Playbooks
