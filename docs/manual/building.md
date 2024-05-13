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
:::{tab-item} MacOS

Install LLVM via [Homebrew](https://brew.sh/):

```sh
brew install llvm
export CXX="/opt/homebrew/opt/llvm/bin/clang++"
export CC="/opt/homebrew/opt/llvm/bin/clang"
export LDFLAGS="-L/opt/homebrew/opt/llvm/lib/c++ -Wl,-rpath,/opt/homebrew/opt/llvm/lib/c++"
```

:::
:::{tab-item} Windows

TODO

:::
:::{tab-item} Ubuntu 20.04

Requires an upgraded GCC version:

```sh
sudo add-apt-repository ppa:ubuntu-toolchain-r/test
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
