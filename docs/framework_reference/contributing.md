# Contributing

## Getting Access to the DESY GitLab

While Pull Requests can be opened in the [GitHub repositories](https://github.com/constellation-daq/Constellation), DESY's
GitLab instance is used as the primary issue tracker. Access to the
[Constellation group](https://gitlab.desy.de/constellation) on can be gained by following these steps:

1. Log in at [https://gitlab.desy.de/users/sign_in](https://gitlab.desy.de/users/sign_in) via Helmholtz AAI using either a research institute or GitHub account.
2. Contact [gitlab.service@desy.de](mailto://gitlab.service@desy.de) with [simon.spannagel@desy.de](mailto://simon.spannagel@desy.de) and [stephan.lachnit@desy.de](mailto://stephan.lachnit@desy.de) in CC stating that you need GitLab access for a common project ([https://gitlab.desy.de/constellation](https://gitlab.desy.de/constellation)).

## Setting up the Development Environment

It is highly recommended to create a virtual Python environment for development.

### Pre-commit Hooks

When contributing to the development of Constellation, following the coding style can be ensured by setting up and activating
git pre-commit hooks which check the code against a variety of code validation tools automatically when you commit your code.

Constellation uses the `pre-commit` tool which registers, updates and runs these hooks automatically. It can be installed and
activated via

```sh
pip install pre-commit
pre-commit install --install-hooks
```

### Development Dependencies

::::{tab-set}
:::{tab-item} C++
:sync: cxx

Meson downloads the required dependencies automatically if they are not found on the system.
System-wide installations of the dependencies can reduce the compilation time significantly.

- ccache + how to setup CXX
- meson dependencies, see docker images

:::
:::{tab-item} Python
:sync: python

```sh
pip install --no-build-isolation -e ".[dev,test]"
```

:::
::::

## Running the Test Suite

::::{tab-set}
:::{tab-item} C++
:sync: cxx

Constellation implements an extensive C++ test suite using [Catch2](https://github.com/catchorg/Catch2/) which needs to be
explicitly enabled at compile time via:

```sh
meson configure build -Dcxx_tests=enabled
```

The unit tests can then be executed via:

```sh
meson test -C build
```

:::
:::{tab-item} Python
:sync: python

Constellation implements an extensive test suite using `pytest` which can be executed by running:

```sh
pytest
```

:::
::::

### Running Coverage Checks

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

To install the documentation dependencies, run:

```sh
pip install --no-build-isolation -e ".[docs]"
```

For generating the code documentation and user manual, also Make, [doxygen](https://doxygen.nl/) and plantuml are required.

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
