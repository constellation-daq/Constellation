# Building from source

The code can be obtained from [the Constellation repository](https://gitlab.desy.de/constellation/constellation).

## Requirements

- Meson 0.61 or newer (see e.g. [Getting Meson](https://mesonbuild.com/Getting-meson.html))
- GCC 12 or newer (see e.g. [Installing GCC](https://gcc.gnu.org/install/))

## Building with unit tests

If you want to test that everything works as intended, you can instead chose to compule with enabled unit testing. In this case replace the steps above with

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

- [doxygen](https://www.doxygen.nl/manual/install.html)
- [plantuml](https://plantuml.com/starting)
- A few [Python packages](https://packaging.python.org/en/latest/guides/section-install/):\
`ablog breathe myst-parser sphinx sphinx_design sphinxcontrib.plantuml`

To create the documentation, run:

```sh
make -C docs/ doxygen
make -C docs/ html
```

The resulting webpage can be accessed via `docs/build/html/index.html`.
