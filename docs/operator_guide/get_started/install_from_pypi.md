# Installing from PyPI

Constellation is available on [PyPI](https://pypi.org/project/ConstellationDAQ/) and can be installed via `pip` with:

```sh
pip install ConstellationDAQ
```

```{note}
Constellation requires Python 3.11 or newer.
```

To install optional components of the framework, you can install those by adding them in squared brackets.
A recommended installation includes the `cli` and `influx` components:

```sh
pip install "ConstellationDAQ[cli,influx]"
```

```{note}
Currently, only the Python version of the framework is available on PyPI.
```
