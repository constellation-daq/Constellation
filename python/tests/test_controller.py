"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2
"""

import time

import pytest

from constellation.core.configuration import flatten_config
from constellation.core.message.cscp1 import SatelliteState

# %%%%%%%%%%%%%%%
# TESTS
# %%%%%%%%%%%%%%%


@pytest.mark.forked
def test_controller_instantiation(mock_controller):
    """Test that we can create the controller."""
    assert mock_controller.name == "BaseController.mock_controller"


@pytest.mark.forked
def test_satellite_access_via_array(mock_controller, mock_satellite):
    """Test state cmd reception via satellite array."""
    timeout = 2
    while timeout > 0 and len(mock_controller.constellation.satellites) < 1:
        time.sleep(0.05)
        timeout -= 0.05
    res = mock_controller.constellation.Satellite.mock_satellite.get_state()
    assert res.msg == "NEW"


@pytest.mark.forked
def test_satellite_hb_state(mock_controller, mock_satellite):
    """Test heartbeat state check."""
    timeout = 4
    while timeout > 0 and len(mock_controller.states) < 1:
        time.sleep(0.05)
        timeout -= 0.05
    assert timeout > 0, "Timed out while waiting for Satellite to be found."
    assert mock_controller.states["Satellite.mock_satellite"] == SatelliteState.NEW


@pytest.mark.forked
def test_satellite_init_w_fullcfg(mock_controller, mock_example_satellite, rawconfig):
    """Test cmd reception."""
    satellite = mock_example_satellite
    timeout = 2
    while timeout > 0 and len(mock_controller.constellation.satellites) < 1:
        time.sleep(0.05)
        timeout -= 0.05
    res = mock_controller.constellation.MockExampleSatellite.mock_satellite.initialize(rawconfig)
    assert res.msg == "transitioning"
    time.sleep(0.1)
    assert satellite.config._config == flatten_config(rawconfig, "MockExampleSatellite", "mock_satellite")
    assert "current_limit" not in satellite.config._config, "Found unexpected item in Satellite cfg after init"
