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
- [Qt](https://www.qt.io/) 5 or 6 (optional, required for GUI components)

The prerequisites can be installed as follows:

::::{tab-set}
:::{tab-item} Debian/Ubuntu

Starting with Ubuntu 24.04 and Debian 12 or newer, the official packages can be used:

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

```{note}
Since Ubuntu 22.04 does not offer Qt6, `build_gui` needs to be set to `qt5` (see also [Build Options](#build-options)).
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

```{note}
Since Ubuntu 20.04 does not offer Qt6, `build_gui` needs to be set to `qt5` (see also [Build Options](#build-options)).
```

```{warning}
Ubuntu 20.04 is not officially supported.
```

:::
:::{tab-item} Fedora

For Fedora the official packages can be used:

```sh
sudo dnf install meson gcc-c++
sudo dnf install qt6-qtbase-devel
```

:::
:::{tab-item} Alma

AlmaLinux 9 requires the CRB repository for Meson, which can be enabled via:

```sh
sudo dnf install epel-release
sudo crb enable
```

AlmaLinux 9 also requires a newer version of GCC than available by default:

```sh
sudo dnf install meson gcc-toolset-14
sudo dnf install qt5-qtbase-devel
```

To use the latest GCC a terminal session has to be started with the `scl` command:

```sh
scl enable gcc-toolset-14 bash
```

```{note}
Since AlmaLinux 9 does not offer Qt6, `build_gui` needs to be set to `qt5` (see also [Build Options](#build-options)).
```

:::
:::{tab-item} MacOS

Building on MacOS requires the installation of the XCode Command Line Tools via:

```sh
xcode-select --install
```

Additionally, the compiler toolchain from [Homebrew](https://brew.sh/) is required:

```sh
brew install meson llvm lld
brew install qt@6
```

``` sh
export CXX="$(brew --prefix)/opt/llvm/bin/clang++"
export CXX_LD="lld"
export CC="$(brew --prefix)/opt/llvm/bin/clang"
export CC_LD="lld"
export LDFLAGS="-L$(brew --prefix)/opt/llvm/lib/c++ -L$(brew --prefix)/opt/llvm/lib/unwind -lunwind"
```

:::
:::{tab-item} Windows

TODO

:::
::::

## Building the C++ Version

```sh
meson setup build -Dbuildtype=debugoptimized -Dwerror=false
meson compile -C build
```

This is already it!

## Installing the C++ Version

For some use-cases, like external satellites, Constellation needs to be installed explicitly system-wide after compilation.
This can be done via:

```sh
meson install -C build
```

Details on installing Constellation for developing satellites can be found in the
[Application Developer Guide](../../application_development/tutorials/templates.md#installing-constellation).

## Build Options

Build options can be set during the setup via

```sh
meson setup build -Doption=value
```

or after the setup via

```sh
meson configure build -Doption=value
```

Relevant build options are:

- `buildtype`: \
  Build type for the C++ version. \
  Possible values are `debug`, `debugoptimized` and `release`. \
  Defaults to `debug`.
- `build_satellites`: \
  Whether to build any satellite or not. \
  Possible values are `true` and `false`. \
  Defaults to `true`.
- `build_gui`: \
  Which Qt version to use for the Graphical User Interface (GUI) library. \
  Possible values are `none` (disables all GUI components), `qt5` and `qt6`. \
  Defaults to `qt6`.
- `cxx_tests`: \
  Whether or not to build the C++ tests. \
  Possible values are `auto`, `enabled` and `disabled`. \
  Defaults to `auto`.
- `cxx_tools`: \
  Whether or not to build a set of C++ debugging tools. \
  Possible values are `true` and `false`. \
  Defaults to `true`.
- `memory_allocator`: \
  Which memory allocator to use (can improve data transmission performance). \
  Possible values are `auto`, `jemalloc`, `mimalloc` and `system`. \
  Defaults to `auto`.

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
:::{tab-item} Alma

```sh
sudo dnf install python3.11
alias python3=python3.11
```

:::
:::{tab-item} Fedora

```sh
sudo dnf install python3
```

:::
:::{tab-item} MacOS

```sh
brew install python
```

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
pip install --no-build-isolation --editable ".[cli,influx]"
```

:::::
::::::
