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
def test_ctrl_reconfigure_cmd_missing(mock_controller, mock_satellite):
    """Test that reconfigure is missing in satellite array when absent in sat."""
    timeout = 2
    while timeout > 0 and len(mock_controller.constellation.satellites) < 1:
        time.sleep(0.05)
        timeout -= 0.05

    with pytest.raises(AttributeError):
        mock_controller.constellation.reconfigure({})
    classarr = getattr(mock_controller.constellation, "Satellite")
    with pytest.raises(AttributeError):
        classarr.reconfigure({})
    sat = getattr(classarr, "mock_satellite")
    with pytest.raises(AttributeError):
        sat.reconfigure({})


@pytest.mark.forked
def test_ctrl_reconfigure_call(mock_controller, mock_example_satellite):
    """Test reconfigure calls via satellite array."""
    timeout = 2
    while timeout > 0 and len(mock_controller.constellation.satellites) < 1:
        time.sleep(0.05)
        timeout -= 0.05

    # cannot call reconfigure when in 'NEW'
    res = mock_controller.constellation.MockExampleSatellite.reconfigure({})
    assert not res["MockExampleSatellite.mock_satellite"].success

    # prepare for reconfigure
    res = mock_controller.constellation.MockExampleSatellite.mock_satellite.initialize({})
    assert res.msg == "transitioning"
    res = mock_controller.constellation.MockExampleSatellite.mock_satellite.launch()
    assert res.msg == "transitioning"

    # sat array does not have reconfigure command
    with pytest.raises(AttributeError):
        mock_controller.constellation.reconfigure({})
    # but class has
    res = mock_controller.constellation.MockExampleSatellite.reconfigure({})
    assert res["MockExampleSatellite.mock_satellite"].msg == "transitioning"
    # and so does the satellite itself
    res = mock_controller.constellation.MockExampleSatellite.mock_satellite.reconfigure({})
    assert res.msg == "transitioning"
    # test error handling:
    # This raises an exception (missing cfg dict as argument)
    with pytest.raises(RuntimeError):
        res = mock_controller.constellation.MockExampleSatellite.reconfigure()


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
