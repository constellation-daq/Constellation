# Contributing

## Getting Access to the DESY GitLab

TODO

## Setting up the Development Environment

We highly recommend creating a virtual Python environment for development.

### Pre-commit Hooks

If you want to develop on Constellation, you can ensure following the coding style by setting up git pre-commit hooks which check your code against a variety of code validation tools automatically when you commit your code.

Install pre-commit via

```sh
pip install pre-commit
pre-commit install --install-hooks
```

### Development Dependencies

::::{tab-set}
:::{tab-item} C++
:sync: cxx

Meson downloads the required dependencies automatically if there are not found on the system.
You can install these to reduce your compilation time.

- ccache + how to setup CXX
- meson deps, see docker images

:::
:::{tab-item} Python
:sync: python

```sh
pip install -e .[dev,test]
```

:::
:::{tab-item} Documentation

```sh
pip install -e .[docs]
```

You will also needs Make, [doxygen](https://doxygen.nl/) and plantuml. TODO.

:::
::::

## Running the Testsuite

::::{tab-set}
:::{tab-item} C++
:sync: cxx

If you want to test that everything works as intended, you can instead chose to compile with enabled unit testing. You can enable those with:

```sh
meson configure build -Dcxx_tests=enabled
```

Now run the included unit tests with:

```sh
meson test -C build
```

:::
:::{tab-item} Python
:sync: python

```sh
pytest
```

:::
::::

### Running Coverage

::::{tab-set}
:::{tab-item} C++
:sync: cxx

Requires [`gcvor`](https://gcovr.com) and compilation with GCC.

```sh
pip install gcovr
```

To create a coverage report, run:

```sh
meson setup build_cov -Dcxx_tests=enabled -Db_coverage=true
meson test -C build_cov
ninja -C build_cov coverage-html
```

:::
:::{tab-item} Python
:sync: python

TODO

:::
::::

## Building the Documentation

Run once or whenever you change C++ source code documentation:

```sh
make -C docs doxygen
```

To build the website, run:

```sh
make -C docs html
```

You can find the homepage of the website in `docs/build/html/index.html`, which you can open via:

```sh
open docs/build/html/index.html
```

If you encounter issue, try running:

```sh
make -C docs clean
```

### Adding Blog Posts

TODO
