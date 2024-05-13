# Contributing

## Pre-commit hooks

If you want to develop on Constellation, you can ensure following the coding style by setting up git pre-commit hooks which check your code against a variety of code validation tools automatically when you commit your code.

Install [pre-commit](https://pre-commit.com) and run `pre-commit install` once inside the Constellation repository to set up the pre-commit hooks for your local git clone.

Note: requires [pre-commit/identify](https://github.com/pre-commit/identify) version > 2.5.20

You will also need to install the validation tools, such as `black` and `flake8` for Python.
