# Installing Constellation

Constellation can be installed from a variety of sources, the optimum might differ depending on the application scenario and
the required components. For graphical user interfaces, the
[Flatpak installation](#installing-as-flatpak-package-from-flathub) is recommended, for the integration into a Python virtual
environment, the [PyPI package](#installing-from-pypi) can be used, and individual satellites can be started directly using
the [Docker container](#running-satellites-with-docker).

## Installing as Flatpak Package from Flathub

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

### Usage Notes

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


## Installing from PyPI

Constellation is available on [PyPI](https://pypi.org/project/ConstellationDAQ/) and can be installed via `pip` with:

```sh
pip install ConstellationDAQ
```

```{note}
Constellation requires Python 3.11 or newer.
```

To install optional components of the framework, you can install those by adding them in squared brackets.
A recommended installation includes the `cli` and `influx` components:

```sh
pip install "ConstellationDAQ[cli,influx]"
```

```{note}
Currently, only the Python version of the framework is available on PyPI.
```

## Running Satellites with Docker

Constellation is available as Docker image, allowing to easily run satellites in a container. The images contains all
framework satellites. Graphical user interfaces are not available in the image.

```{hint}
It is also possible to use [podman](https://podman.io/docs/installation) instead of docker, which is easier to install.
```

A satellite can be started via:

```sh
docker run --network host -it gitlab.desy.de:5555/constellation/constellation/constellation:latest
```

```{attention}
Without `--network host` network discovery does not work.
```

In the [container registry](https://gitlab.desy.de/constellation/constellation/container_registry), two images are available:

- `constellation`: based on Ubuntu 24.04
- `constellation_ci`: based on Fedora, containing development files and tools for building external C++ satellites

The following tags are available for each images:

- `latest`: tag pointing to the last released version
- `vX.Y.Z`: tags for each release starting from version 0.6
- `nightly`: tag pointing to the last nightly build from the main branch (not recommended for production use)

```{note}
Currently, only C++ satellites are available in the Docker images.
```
