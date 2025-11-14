# Installing from Flathub

Constellation releases are packaged as so-called *Flatpaks* and are published on
[Flathub](https://flathub.org/apps/de.desy.constellation), an app store for Linux-based operating systems.
Flatpaks are supported by almost all Linux distributions and detailed installation instructions tailored to each distribution
can be found for example on the [Flathub setup page](https://flathub.org/setup).

After Flatpak has been installed and the Flathub repository has been set up, Constellation can be installed either via the
graphical application installer of the desktop environment or on the command line by running:

```sh
flatpak install flathub de.desy.constellation
```

This installs all graphical user interfaces of Constellation as well as the satellites maintained in the main COnstellation
repository. The graphical user interfaces such as controllers and listeners show up in the application search of the desktop
environment just as any other application installed through the package manager of the system.

```{note}
After installing the Constellation Flatpak, the system requires a restart in order for Constellation to show up in the application search of the desktop environment.
```

## Usage Notes

Flatpak is mainly designed for the distribution of graphical user interfaces, not necessarily command-line tools. They can,
hwoever, still be started, albeit only from the command line and not the desktop environment, In order to start a satellite,
the following command can to be used:

```sh
flatpak run de.desy.constellation -t Sputnik -n Flatpak -g edda
```

```{note}
Currently, only C++ satellites are available on Flathub.
```

Flatpak applications run in a sandbox with limited permissions to the file system. By default, Constellation can
only read and write to data in the user's home folder, which means that writing to `/data` with data receivers will fail.
However, file system permissions for specific paths like `/data` can be added to the permission set of the installed
Constellation Flatpak using e.g. the [Flatseal](https://flathub.org/apps/com.github.tchx84.Flatseal) tool.

It should be noted that a Flatpak is not suited for application development, for which an
[installation from source](../../application_development/intro/install_from_source.md) is required. Information on how to build the Constellation Flatpak from
source can be found in the [framework reference](../../framework_reference/flatpak.md).
