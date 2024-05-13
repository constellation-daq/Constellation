# Contributing

## Getting Access to the DESY GitLab

TODO

## Setting up the Development Environment

### Pre-commit Hooks

If you want to develop on Constellation, you can ensure following the coding style by setting up git pre-commit hooks which check your code against a variety of code validation tools automatically when you commit your code.

Install [pre-commit](https://pre-commit.com) and run `pre-commit install` once inside the Constellation repository to set up the pre-commit hooks for your local git clone.

Note: requires [pre-commit/identify](https://github.com/pre-commit/identify) version > 2.5.20

You will also need to install the validation tools, such as `black` and `flake8` for Python.

### Optional C++ Dependencies

TODO

### Dependencies for the Documentation

Ensure you have a valid Python virtual environment as described in [Building the Source](../manual/building.md).
Then, install the dependencies to build the documentation:

```sh
pip install -e .[docs]
```

You will also needs Make, [doxygen](https://doxygen.nl/) and plantuml. TODO.

## Running the Testsuite

TODO: tabs for C++ and Python

If you want to test that everything works as intended, you can instead chose to compile with enabled unit testing. You can enable those with:

```sh
meson configure build -Dcxx_tests=enabled
```

Now run the included unit tests with:

```sh
meson test -C build
```

### Running Coverage

To create a coverage report with [`gcvor`](https://gcovr.com), run:

```sh
meson setup build_cov -Dcxx_tests=enabled -Db_coverage=true
meson test -C build_cov
ninja -C build_cov coverage-html
```

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
