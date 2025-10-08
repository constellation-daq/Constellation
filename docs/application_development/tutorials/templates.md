# Creating a New Satellite

Constellation provides [satellite templates](https://gitlab.desy.de/constellation/templates) for Python and for C++ with the
[Meson](https://mesonbuild.com/) and [CMake](https://cmake.org/) build systems, which can be used as a starting point.

```{seealso}
This tutorial will create an external satellite. For more details on the difference between external and framework
satellites, consult the [introduction](../intro/listing.md).
```

## Getting Started

Template repositories are provided on both GitLab and GitHub and can be cloned or used directly.

On GitLab, the repository should be forked and renamed, while on GitHub, new repositories can be created from the template
repositories using the green {bdg-primary}`Use this template` button. Alternatively, for projects which will not be hosted on GitHub, the
template code can be downloaded manually using the green {bdg-primary}`Code` button.

The following templates are available:

* Satellite in **C++ with Meson** build system:

  * [https://github.com/constellation-daq/template-satellite-cpp-meson](https://github.com/constellation-daq/template-satellite-cpp-meson)
  * [https://gitlab.desy.de/constellation/templates/satellite-cpp-meson](https://gitlab.desy.de/constellation/templates/satellite-cpp-meson).

* Satellite in **C++ with CMake** build system:

  * [https://github.com/constellation-daq/template-satellite-cpp-cmake](https://github.com/constellation-daq/template-satellite-cpp-cmake).
  * [https://gitlab.desy.de/constellation/templates/satellite-cpp-cmake](https://gitlab.desy.de/constellation/templates/satellite-cpp-cmake).

* Satellite in **Python**:

  * [https://github.com/constellation-daq/template-satellite-python](https://github.com/constellation-daq/template-satellite-python).
  * [https://gitlab.desy.de/constellation/templates/satellite-python](https://gitlab.desy.de/constellation/templates/satellite-python).

After cloning the repository, the files should first be adapted to the desired
[satellite type](../../operator_guide/concepts/satellite.md#type-and-name). This can be accomplished using the supplied
rename script:

```sh
./rename-template.py NewType
```

The rename script will adjust and rename all relevant files and finally deletes itself.

```{seealso}
Before committing the changes, it is recommended to install the provided pre-commit hooks as described in the
[introduction](../intro/tools.md#using-pre-commit).
```

## Installing Constellation

```{seealso}
The installation instructions in the [operator guide](../../operator_guide/get_started/install_from_source.md) can be
consulted to install build dependencies such as Meson or Python's `venv` module.
```

::::::{tab-set}
:::::{tab-item} C++ / Meson
:sync: cxx

External satellites can use a [wrap file](https://mesonbuild.com/Wrap-dependency-system-manual.html) for Constellation to
automatically build Constellation if required. The Meson template satellite already contains a wrap file pointing to the
current version.

```{attention}
The wrap file does not automatically update when a new version is released. It is possible to updated it manually,
for example by downloading the current file from the template repository again.
```

Alternatively, Constellation can also be installed system-wide as explained in the instructions for CMake.

:::::
:::::{tab-item} C++ / CMake
:sync: cxx-cmake

External satellites require an installation from source of Constellation. By default, the `meson install` command installs to
`/usr/local`. This can be changed via:

```sh
meson configure build -Dprefix=CNSTLN_PREFIX # set installation directory here, e.g. `$(pwd)/usr`
meson install -C build
```

````{note}
Constellation exports its dependency using `pkg-config`, which can be easily used in many build systems, including CMake.
In order to find Constellation via `pkg-config` in a non-standard location when building external satellites, the prefix path
needs to be exported:

```sh
export CNSTLN_PREFIX=$(pwd)/usr
export PKG_CONFIG_PATH="$CNSTLN_PREFIX/lib64/pkgconfig:$CNSTLN_PREFIX/usr/share/pkgconfig"
```

Note that the platform specific part (`lib64`) might be different depending on your platform, e.g. it is
`lib/x86_64-linux-gnu` for `amd64` Debian/Ubuntu. It can be found be checking the content of the prefix path.
````

:::::
:::::{tab-item} Python
:sync: python

Constellation needs be installed in a virtual environment via pip, which will happen automatically using editable installs.

A virtual environment can be created via:

```sh
python3 -m venv venv
source venv/bin/activate
```

:::::
::::::

## Building the Satellite

::::::{tab-set}
:::::{tab-item} C++ / Meson
:sync: cxx

The satellite can be built using:

```sh
meson setup build
meson compile -C build
```

:::::
:::::{tab-item} C++ / CMake
:sync: cxx-cmake

The satellite can be built using:

```sh
cmake -B build -G Ninja
ninja -C build
```

Or alternatively using Make:

```sh
cmake -B build
make -C build
```

:::::
:::::{tab-item} Python
:sync: python

The satellite can be installed in the virtual environment via:

```sh
pip install -e .
```

:::::
::::::

After building one can check that the satellite executable is working correctly:

::::{tab-set}
:::{tab-item} C++
:sync: cxx

```sh
./build/SatelliteNewType
```

:::
:::{tab-item} Python
:sync: python

```sh
SatelliteNewType
```

:::
::::

```{note}
`NewType` has to be replaced with the satellite type used when renaming.
```

## Implementing Functionality

There are separate implementation guides of the base functionality of a satellite for [C++](satellite_cxx.md) and
[Python](satellite_py.md). Separate guide exist for adding functionality such as [logging](../functionality/logging.md),
[metrics](../functionality/metrics.md), [data transmission](../functionality/data_transmission.md) and
[custom commands](../functionality/custom_commands.md).

Once the functionality has been implemented and tested, the README should be updated and included in the
[satellite listing](../intro/listing.md#listing-an-external-satellite-in-the-library).
If applicable, the satellite can also be added to the [main repository](../howtos/migrate_external_satellite.md).
