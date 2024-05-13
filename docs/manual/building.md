# Building from source

The code can be obtained from [the Constellation repository](https://gitlab.desy.de/constellation/constellation).

## Requirements

- Meson 0.61 or newer (see e.g. [Getting Meson](https://mesonbuild.com/Getting-meson.html))
- GCC 12 or newer (see e.g. [Installing GCC](https://gcc.gnu.org/install/))

## Standard build

To build, simply run:

```sh
meson setup build
meson compile -C build
```

### On Ubuntu 20.04

The default compiler needs to be updated.

```sh
sudo add-apt-repository ppa:ubuntu-toolchain-r/test
sudo apt update
sudo apt install g++-13
```

Then the compiler version needs to be explicitly set before building:

```sh
CXX=g++-13 CC=gcc-13 meson setup build
meson compile -C build
```

## Installing the Python package

To install the package, run the following command inside the root of the cloned repository:

```sh
pip install -e .
```
