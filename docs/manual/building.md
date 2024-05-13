
Start by obtaining the code from [the Constellation repository](https://gitlab.desy.de/constellation/constellation).

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

## Building with unit tests

If you want to test that everything works as intended, you can instead chose to compule with enabeled unit testing. In this case replace the steps above with

```sh
meson setup build -Dcxx_tests=enabled
meson compile -C build
```
(or use `meson setup build -Dcxx_tests=enabled --reconfigure` if you had already configured without unit testing before)

Now run the included unit tests with:

```sh
meson test -C build
```

To create a coverage report with [`gcvor`](https://gcovr.com), run:

```sh
meson setup build_cov -Db_coverage=true
meson test -C build_cov
ninja -C build_cov coverage-html
```


## Documentation

Building the documentation requires
* [doxygen](https://www.doxygen.nl/manual/install.html)
* [plantuml](https://plantuml.com/starting)
* A few [Python packages](https://packaging.python.org/en/latest/guides/section-install/):\
`ablog breathe myst-parser sphinx sphinx_design sphinxcontrib.plantuml`

To create the documentation, run:

```sh
make -C docs/ doxygen
make -C docs/ html
```

The resulting webpage can be accessed via `docs/build/html/index.html`.
