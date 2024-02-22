# Building from Source

## Requirements

- Meson 0.61 or newer
- GCC 12 or newer

## Building

To build, simply run:

```sh
meson setup build
meson compile -C build
```

## Unit tests

Unit tests can be run with:

```sh
meson test -C build
```

To create a coverage reports with [`gcvor`](https://gcovr.com), run:

```sh
meson setup build_cov -Db_coverage=true
meson test -C build_cov
ninja -C build_cov coverage-html
```

## Pre-commit hooks

If you want to develop on Constellation, you can ensure following the coding style by setting up git pre-commit hooks which check your code against a variety of code validation tools automatically when you commit your code.

Install [pre-commit](https://pre-commit.com) and run `pre-commit install` once inside the Constellation repository to set up the pre-commit hooks for your local git clone.

You will also need to install the validation tools, such as `black` and `flake8` for Python.

## Documentation

Building the documentation requires doxygen and some Python packages, which can be installed via:

```sh
python3 -m venv venv
source venv/bin/active
pip install sphinx myst-parser sphinx_immaterial breathe
```

Finally run:

```sh
make -C docs/ doxygen
make -C docs/ html
```

The resulting webpage can be accessed via `docs/build/html/index.html`.
