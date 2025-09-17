# Satellites as systemd services

When running an experiment, typically some data has to be monitored continuously.
It is therefore a common requirement to run satellites as a service in the background.

Most Linux distributions by now use [systemd](https://systemd.io/) as a system service manager.
It takes care of running processes in the background, logging their standard and error output and can also be configured to notify a system administrator in the case of an unexpected termination of the process.

Running a satellite as a systemd service is as easy as creating a simple `.service` text file and placing it in the right directory.
There are two different types of systemd services: user services and system services.
User services can be created without super user privileges but are typically bound to a user session which might terminate unexpectedly.
System services should therefore be preferred, if possible.

## Prerequisites

It is assumed, that the satellite to be run as systemd service is installed within a python virtual environment at `/home/USERNAME/path/to/venv`.

## Writing the systemd service file

Since multiple satellites of the same type but with different names can exist within Constellation, it makes sense to use systemd's [service template syntax](https://www.freedesktop.org/software/systemd/man/latest/systemd.service.html#Service%20Templates).
It allows to pass one additional parameter to the service when starting it, which can then be accessed within the service file via `%i`.

A typical service file should then look like this:

```systemd
[Unit]
Description=Constellation Satellite

[Service]
Type=exec
User=USERNAME
EnvironmentFile=/home/USERNAME/some/known/path/env
ExecStart=/home/USERNAME/path/to/venv/bin/SatelliteSomething -g $CONSTELLATION_GROUP -n %i
```

To use systemd's template syntax, it has to be saved as `SatelliteSomething@.service` - the important part being the `@` at the end of the name.

- `Description` should be adapted for the specific satellite.
- `User` is required to run the service as a non-root user. `USERNAME` needs to
  be replaced with the user that created the python virtual environment in
  which the satellite is installed.
- [`EnvironmentFile`](https://www.freedesktop.org/software/systemd/man/latest/systemd.exec.html#EnvironmentFile=)
  allows to specify a file containing additional environment variables to load.
  The file `/home/USERNAME/some/known/path/env` should contain a single line
  `CONSTELLATION_GROUP=YOUR_GROUPNAME`. This way, `$CONSTELLATION_GROUP` can be
  defined dynamically without modifying the service file. It is of course also
  possible to hard code the constellation group and remove that line.
- `ExecStart` is then the command to be executed when the service is started.
  It should point to the satellite within the python virtual environment. The
  constellation group is read from the environment and the satellite name from
  the `%i` service template parameter.

To run the satellite as system service, the file has to be written to `/etc/systemd/system/`.
To use it as a user service, it has to be written to `~/.local/share/systemd/user/`.
When used as a user service, the `User=USERNAME` line should be removed, it will be started as the user that created the service.

## Using the service

If the service file was just written, it is necessary to first reload the systemd unit list:

```sh
systemctl daemon-reload
```

or

```sh
systemctl --user daemon-reload
```

depending on whether a system or user service is being used.

A system service can then be started by running the following as root:

```sh
systemctl start SatelliteSomething@SatelliteName.service
```

As a user service, the command looks similar but doesn't require super user privileges:

```sh
systemctl start --user SatelliteSomething@SatelliteName.service
```

Other useful commands are `systemctl stop` to stop the service, `systemctl status` to display its current status and the last lines of its output and `systemctl enable` to automatically start the service on startup. The `--user` flag needs to be added to every command in the case a user service is used.

Note that systemd only manages the satellite process. A constellation controller is still needed to actually initialize and launch the satellite.

## Managing a system service as a user

To not require super user privileges to start the constellation satellites, it might be useful to allow a specific non-privileged user to manage the satellite services.

On most Linux distributions, this can be achieved using a polkit rule such as:

```javascript
polkit.addRule(function(action, subject) {
    if (action.id == "org.freedesktop.systemd1.manage-units" &&
        RegExp('^Satellite').test(action.lookup("unit")) === true &&
        subject.user == "USERNAME") {
        return polkit.Result.YES;
    }
});
```

This has to be saved as `anything.rules` to `/etc/polkit-1/rules.d/`. It allows the user `USERNAME` to manage any systemd service whose name starts with `Satellite`.
Of course, this also requires consistent service file names such that every satellite service starts with `Satellite`.
