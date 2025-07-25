# Building External Satellites

External satellites are satellites that have their implementation not in the Constellation repository, but in some downstream
code base. This is typically the case for highly specialized satellites such as detector prototypes.

## Installing Constellation

```{seealso}
The installation instructions in the [operator guide](../../operator_guide/get_started/install_from_source.md) can be
consulted to install build dependencies such as Meson or Python's `venv` module.
```

::::{tab-set}
:::{tab-item} C++
:sync: cxx

External satellites usually require an installation from source of Constellation. By default, the `meson install` command
installs to `/usr/local`. This can be changed via:

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
`lib/x86_64-linux-gnu` for Debian/Ubuntu. It can be found be checking the content of the prefix path.
````

:::
:::{tab-item} Python
:sync: python

Constellation needs be installed in a virtual environment via pip, which will happen automatically using editable installs.

A virtual environment can be created via:

```sh
python3 -m venv venv
source venv/bin/activate
```

:::
::::

## Starting from a Satellite Template

Constellation provides [template satellites](https://gitlab.desy.de/constellation/templates) for Python and for C++ with the
[Meson](https://mesonbuild.com/) and [CMake](https://cmake.org/) build systems, which can be used as a starting point.
On GitHub, new repositories can be created from the template repositories using the green `Use this template` button.

::::::{tab-set}
:::::{tab-item} C++
:sync: cxx

::::{tab-set}
:::{tab-item} Meson

The template repository can be found on [GitHub](https://github.com/constellation-daq/template-satellite-cpp-meson).

The satellite can be built using:

```sh
meson setup build
meson compile -C build
```

:::
:::{tab-item} CMake

The template repository can be found on [GitHub](https://github.com/constellation-daq/template-satellite-cpp-cmake).

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

:::
::::

:::::
:::::{tab-item} Python
:sync: python

The template repository can be found on [GitHub](https://github.com/constellation-daq/template-satellite-python).

The satellite can be installed in the virtual environment via:

```sh
pip install -e .
```

:::::
::::::

## Making Modifications

TODO:

- Install pre-commit hooks
- Rename satellite
- Adding dependencies
- Adjusting README
- Listing satellites in the library

## Continuous Integration

TODO:

- Description of the CI jobs
