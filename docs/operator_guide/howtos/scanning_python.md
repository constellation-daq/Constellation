# Parameter Scans with Python

The Python Controller of Constellation is a fully-features IPython console. It can be installed with the `cli` component:

::::{tab-set}
:::{tab-item} PyPI
:sync: pypi

```sh
pip install ConstellationDAQ[cli]
```

:::
:::{tab-item} Source
:sync: source

```sh
pip install --no-build-isolation -e .[cli]
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
constellation.launch()

# Start a parameter scan for the key "interval"
for ivl in range(0, 100, 10):
    # Reconfigure one of the satellites to the new parameter value
    recfg = {"interval": ivl}
    constellation.Sputnik.reconfigure(recfg)

    # Wait until ll states are back in the ORBIT state
    ctrl.await_state(SatelliteState.ORBIT)

    # Repeat this measurement four times:
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

It is also possible to create a standalone script which can be run without the IPython console:

```python
import time

from constellation.core.configuration import load_config
from constellation.core.controller import BaseController
from constellation.core.logging import setup_cli_logging

# Settings
config_file_path = "/path/to/config.toml"
group_name = "edda"
n_satellites = 1

# Setup logging
setup_cli_logging("info")

# Create controller
ctrl = BaseController(group=group_name, name="PythonCtrl", interface="*")
constellation = ctrl.constellation

# Load configuration
cfg = load_config(config_file_path)

# Wait until all satellites are connected
while len(constellation.satellites) < n_satellites:
    print("Waiting for satellites...")
    time.sleep(0.5)

# Initialize and reconfigure loop goes here as above
```
