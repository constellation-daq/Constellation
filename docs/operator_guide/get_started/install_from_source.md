# Installing from Source

This document describes the procedure of how to install (and possibly compile) the Constellation framework from source.
The source code can be obtained from [the Constellation Git repository](https://gitlab.desy.de/constellation/constellation):

```sh
git clone https://gitlab.desy.de/constellation/constellation.git
```

Constellation has two separate implementations, one in C++ and one in Python. The tabs below discuss the installation
procedure for the two versions separately, but installing both in parallel on the same machine is possible and encouraged.

::::::{tab-set}
:::::{tab-item} C++

## Prerequisites for C++

The C++ version of Constellation requires:

- [Meson](https://mesonbuild.com/) 0.61 or newer
- C++20 capable compiler like GCC 12 or newer and clang 16 or newer
- C++20 enabled standard library (GCC's libstdc++ 12 or newer and LLVM's libc++ 18 or newer)
- [Qt](https://www.qt.io/) 5 or 6 (required for GUI components)

The prerequisites can be installed as follows:

::::{tab-set}
:::{tab-item} Debian/Ubuntu

Starting with Ubuntu 24.04 and Debian 12 or newer, the official packages for GCC and Meson can be used:

```sh
sudo apt install meson g++
sudo apt install qt6-base-dev
```

Ubuntu 22.04 requires a newer version of GCC than installed by default. Version 12 is recommended and available in the
regular package repositories:

```sh
sudo apt install meson g++-12
sudo apt install qtbase5-dev
export CXX="g++-12"
```

Ubuntu 20.04 requires newer versions of GCC and Meson than available from the standard package repositories. They are available in official PPAs:

```sh
sudo add-apt-repository ppa:ubuntu-toolchain-r/test
sudo add-apt-repository ppa:ubuntu-support-team/meson
sudo apt update
sudo apt install meson g++-13
sudo apt install qtbase5-dev
export CXX="g++-13"
```

:::
:::{tab-item} ALMA/Fedora

```sh
sudo dnf install meson clang
sudo dnf install qt6-qtbase-devel
export CXX=clang++
```

:::
:::{tab-item} MacOS 14

MacOS requires an installation of Meson and LLVM, e.g. via [Homebrew](https://brew.sh/):

```sh
brew install meson
brew install llvm
brew install qt@6
```

Assuming `${HOMEBREW_PREFIX}` is set (likely `/opt/homebrew`, can otherwise be found by typing e.g. `which meson`):

``` sh
export CXX="${HOMEBREW_PREFIX}/opt/llvm/bin/clang++"
export CC="${HOMEBREW_PREFIX}/opt/llvm/bin/clang"
export LDFLAGS="-L${HOMEBREW_PREFIX}/opt/llvm/lib/c++ -Wl,-rpath,${HOMEBREW_PREFIX}/opt/llvm/lib/c++"
export CXXFLAGS="-fexperimental-library -DASIO_HAS_SNPRINTF"
```

:::
:::{tab-item} Windows

TODO

:::
::::

## Building the C++ Version

```sh
meson setup build -Dbuildtype=debugoptimized
meson compile -C build
```

This is already it!

## Meson Configuration Options

This section describes the configuration options specific to Constellation, their purposes, and how to use them.

# Build Options

buildtype

**Purpose**:
Specify the build type to use.

**Values**:
- ``plain``
- ``debug``
- ``debugoptimized``
- ``release``
- ``minsize``
- ``custom``

**Default**: ``debug``

**Usage**:
Set to control the balance between debugging and performance.

```
meson setup build -Dbuildtype=release         # Build with full optimization
meson setup build -Dbuildtype=debugoptimized  # Debugging with optimization
```

build_gui

**Purpose**:
Build Qt graphical UI's, and selecting the relevant Qt version.

**Values**:
- ``none`` (no GUI)
- ``qt5`` (use qt5)
- ``qt6`` (use qt6)

**Default**: ``qt6``

**Usage**:
set to enable/disable GUI builds or choose qt version.

```
meson setup build -Dbuild_gui=none  # Disable GUI's
meson setup build -Dbuild_gui=qt5   # Use qt5
```

cxx_tests

**Purpose**:
Control whether C++ tests are built or not (requires catch2).

**values**:
- ``enabled``   (always build)
- ``disabled``  (never build)
- ``auto``      (build if dependencies are met)

**Default**: ``auto``

**Usage**:
Set to enabled for testing, disabled to skip tests, or maybe auto for building only if dependencies are met.

```
meson setup build -Dcxx_tests=disabled  # Skip C++ tests
```

cxx_tools

**Purpose**:
Enables/Disables building C++ tools.

**Values**:
- ``true``  (build)
- ``false`` (don't build)

**Default**: ``true``

**Usage**:
Set to true for including the tools otherwise to false for excluding them.

## Installing the C++ Version

For some use-cases, like external satellites, Constellation needs to be installed explicitly system-wide after compilation.
This can be done via:

```sh
meson install -C build
```

For details on installing Constellation for external satellites, see
[Building External Satellites](../../application_development/howtos/external_satellite.md).

:::::
:::::{tab-item} Python

## Prerequisites for Python

The Python version of Constellation requires

- Python 3.11 or newer
- The Python [`venv`](https://docs.python.org/3/library/venv.html) module

The prerequisites can be installed as follows:

::::{tab-set}
:::{tab-item} Debian/Ubuntu

```sh
sudo apt install python3 python3-venv
```

:::
:::{tab-item} ALMA/Fedora

TODO

:::
:::{tab-item} MacOS

TODO

:::
:::{tab-item} Windows

TODO

:::
::::

## Setting up a Python Virtual Environment

```sh
python3 -m venv venv
source venv/bin/activate
pip install meson-python meson ninja
```

## Installing the Constellation Package

```sh
pip install --no-build-isolation --editable .
```

To install optional components of the framework, you can install those by replacing `.` with `.[component]`.
A recommended installation includes the `cli` and `influx` components:

```sh
pip install --no-build-isolation --editable .[cli,influx]
```

:::::
::::::
