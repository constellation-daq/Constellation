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

To build the documentation, run:
```bash
sudo apt install libclang-dev  # note down version, e.g. 17.0.4
python3 -m venv venv
source venv/bin/active
pip install clang=[VERSION]  # insert clang version
pip install sphinx hawkmoth myst-parser sphinx_immaterial
pip install hawkmoth@git+https://github.com/stephanlachnit/hawkmoth.git@constellation
```

Note: this currently requires LLVM 18.
