# Parameter Scans with Python

## Using the IPython Console

The Python Controller of Constellation is a fully featured IPython console. It can be installed with the `cli` component:

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

    # Wait a bit to ensure Sputnik is in reconfiguring state
    time.sleep(0.1)

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
import time

from constellation.core.controller import ScriptableController
from constellation.core.controller_configuration import load_config
from constellation.core.protocol.cscp1 import SatelliteState

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

## Scanning until a Telemetry Condition is met

In many cases it preferable to start a new run during a scan after a certain target has been reached instead of waiting for a fixed amount of time.
In Constellation [telemetry](../concepts/telemetry.md) can be used for that purpose.

In order to listen to metrics, a custom controller class inheriting both from {py:class}`ScriptableController <core.controller.ScriptableController>` and {py:class}`MonitoringListener <core.listener.MonitoringListener>` needs to be created.
In this class a subscription to the topic of the target metric needs to be set and stored in a variable in the callback:

```python
# Custom controller which listens to metrics
class MyController(ScriptableController, MonitoringListener):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        # Subscribe to POST_VETO metric
        self.set_topics(["STAT/POST_VETO"])
        self.post_veto_triggers = 0

    def receive_metric(self, sender, metric, timestamp, value):
        if metric.name == "POST_VETO" and sender == "AidaTLU.2020":
            self.post_veto_triggers = value
```

With this custom controller the sleep statement can be replaced with a loop checking for a condition:

```python
# Wait until 1M triggers are collected
while ctrl.post_veto_triggers < 1000000:
    time.sleep(1)
```

Full example:

```python
import time

from constellation.core.controller import ScriptableController
from constellation.core.controller_configuration import load_config
from constellation.core.listener import MonitoringListener
from constellation.core.message.cscp1 import SatelliteState


# Custom controller which listens to metrics
class MyController(ScriptableController, MonitoringListener):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        # Subscribe to POST_VETO metric
        self.set_topics(["STAT/POST_VETO"])
        self.post_veto_triggers = 0

    def receive_metric(self, sender, metric, timestamp, value):
        if metric.name == "POST_VETO" and sender == "AidaTLU.2020":
            self.post_veto_triggers = value


# Settings
config_file_path = "/path/to/config.toml"
group_name = "edda"

# Create controller
ctrl = ScriptableController(group_name)
constellation = ctrl.constellation

# Load configuration
cfg = load_config(config_file_path)

# Wait until all satellites are connected
ctrl.await_satellites(["AidaTLU.2020", "Caribou.SPARC", "Keithley.2410"])

# Initialize and launch
constellation.initialize(cfg)
ctrl.await_state(SatelliteState.INIT)
constellation.launch()
ctrl.await_state(SatelliteState.ORBIT)

# Scan over bias voltages
voltages = [-1.2, -2.4, -3.6, -4.8]
for voltage in voltages:
    # Reconfigure Keithley with new voltage
    recfg = {"voltage": voltage}
    constellation.Keithley.reconfigure(recfg)

    # Wait a bit to ensure Keithley is in reconfiguring state
    time.sleep(0.1)

    # Wait until all states are back in the ORBIT state
    ctrl.await_state(SatelliteState.ORBIT)

    # Start the run
    constellation.start(f"voltage{str(voltage).replace('.', '_')}")
    ctrl.await_state(SatelliteState.RUN)

    # Wait until 1M triggers are collected
    while ctrl.post_veto_triggers < 1000000:
        time.sleep(1)

    # Stop the run and await ORBIT state of all satellites
    constellation.stop()
    ctrl.await_state(SatelliteState.ORBIT)

# Land and shutdown satellites
constellation.land()
constellation.shutdown()
```
