# Building from Source

The code can be obtained from [the Constellation repository](https://gitlab.desy.de/constellation/constellation) via git.

```sh
git clone https://gitlab.desy.de/constellation/constellation.git
```

Constellation has two separate implementations, one in C++ and one in Python. You can select the tabs to select which implementation you prefer, and you can also install both at the same time.

::::::{tab-set}
:::::{tab-item} C++

## C++ Requirements

- [Meson](https://mesonbuild.com/) 0.61 or newer
- C++20 capable compiler like GCC 12 or newer and LLVM 16 or newer

::::{tab-set}
:::{tab-item} Debian/Ubuntu

```sh
sudo apt install meson g++
```

:::
:::{tab-item} ALMA/Fedora

```sh
sudo dnf install meson clang
export CXX=clang++
```

:::
:::{tab-item} MacOS 14

Install Meson and LLVM via [Homebrew](https://brew.sh/):

```sh
brew install meson
brew install llvm
```
Assuming `${HOMEBREW_PREFIX}` is set (likely `/opt/homebrew`, can otherwise be found by typing e.g. `which meson`):
``` sh
export CXX="${HOMEBREW_PREFIX}/opt/llvm/bin/clang++"
export CC="${HOMEBREW_PREFIX}/opt/llvm/bin/clang"
export LDFLAGS="-L${HOMEBREW_PREFIX}/opt/llvm/lib/c++ -Wl,-rpath,${HOMEBREW_PREFIX}/opt/llvm/lib/c++"
```

:::
:::{tab-item} Windows

TODO

:::
:::{tab-item} Ubuntu 22.04

Requires an upgraded GCC version:

```sh
sudo apt install meson g++-12
export CXX="g++-12"
```

:::

:::{tab-item} Ubuntu 20.04

Requires an upgraded GCC version:

```sh
sudo add-apt-repository ppa:ubuntu-toolchain-r/test
sudo add-apt-repository ppa:ubuntu-support-team/meson
sudo apt update
sudo apt install meson g++-13
export CXX="g++-13"
```

:::
::::

## Building

```sh
meson setup build
meson compile -C build
```

This is already it!

:::::
:::::{tab-item} Python

## Python Requirements

- Python 3.11 or newer
- [`venv`](https://docs.python.org/3/library/venv.html) module
- HDF5 development libraries

::::{tab-set}
:::{tab-item} Debian/Ubuntu

```sh
sudo apt install python3-venv libhdf5-dev
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

## Setting up the Virtual Environment

```sh
python3 -m venv venv
source venv/bin/activate
pip install meson-python meson ninja
```

## Installing the Package

```sh
pip install --no-build-isolation --editable .
```

:::::
::::::
