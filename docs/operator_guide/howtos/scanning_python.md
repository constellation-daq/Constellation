# Parameter Scans with Python

## Using the IPython Console

The Python Controller of Constellation is a fully-features IPython console. It can be installed with the `cli` component:

::::{tab-set}
:::{tab-item} PyPI
:sync: pypi

```sh
pip install "ConstellationDAQ[cli]"
```

:::
:::{tab-item} Source
:sync: source

```sh
pip install --no-build-isolation -e ".[cli]"
```

:::
::::

Consequently, the full Python syntax can be used to automate data acquisition and more complex tasks such as scanning parameters.
The following is an example script to take measurements for different values of a parameter.

First, the Constellation is initialized and launched, then a parameter scan is started using the
{bdg-secondary}`reconfiguring` state transition described
[in the satellite section](../concepts/satellite.md#changing-states---transitions).

```python
import time

# Initialize and launch the Constellation with the configuration read from a file
constellation.initialize(cfg)
ctrl.await_state(SatelliteState.INIT)
constellation.launch()
ctrl.await_state(SatelliteState.ORBIT)

# Start a parameter scan for the key "interval"
for ivl in range(0, 100, 10):
    # Reconfigure one of the satellites to the new parameter value
    recfg = {"interval": ivl}
    constellation.Sputnik.reconfigure(recfg)

    # Wait until all states are back in the ORBIT state
    ctrl.await_state(SatelliteState.ORBIT)

    # Repeat this measurement four times
    for run in range(1, 4):
        # Start the run
        constellation.start(f"i{ivl}_r{run}")
        ctrl.await_state(SatelliteState.RUN)

        # Run for 15 seconds
        time.sleep(15)

        # Stop the run and await ORBIT state of all satellites
        constellation.stop()
        ctrl.await_state(SatelliteState.ORBIT)
```

The `await_state` function raises an exception after waiting for  60 seconds by default (can be adapted using the `timeout`
parameter) and when any satellite is in the ERROR state.

## Using a Standalone Script

It is also possible to create a standalone script which can be run without the IPython console:

```python
from constellation.core.configuration import load_config
from constellation.core.controller import ScriptableController

# Settings
config_file_path = "/path/to/config.toml"
group_name = "edda"

# Create controller
ctrl = ScriptableController(group_name)
constellation = ctrl.constellation

# Load configuration
cfg = load_config(config_file_path)

# Wait until all satellites are connected
ctrl.await_satellites(["Sputnik.s1", "Sputnik.s2"])

# Initialize and reconfigure loop goes here as above
```
