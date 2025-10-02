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


def test_controller_instantiation(mock_controller):
    """Test that we can create the controller."""
    ctrl, _ctx = mock_controller
    assert ctrl.name == "BaseController.mock_controller"


def test_satellite_access_via_array(mock_controller, mock_satellite):
    """Test state cmd reception via satellite array."""
    satellite, _ctx = mock_satellite
    ctrl, _ctx = mock_controller
    timeout = 2
    while timeout > 0 and len(ctrl.constellation.satellites) < 1:
        time.sleep(0.05)
        timeout -= 0.05
    assert len(ctrl.constellation.satellites) == 1

    res = ctrl.constellation.Satellite.mock_satellite.get_state()
    assert res.msg == "NEW"


def test_ctrl_reconfigure_cmd_missing(mock_controller, mock_satellite):
    """Test that reconfigure is missing in satellite array when absent in sat."""
    ctrl, _ctx = mock_controller
    timeout = 2
    while timeout > 0 and len(ctrl.constellation.satellites) < 1:
        time.sleep(0.05)
        timeout -= 0.05
    assert len(ctrl.constellation.satellites) == 1, "Timed out while waiting for Satellite to be found"

    with pytest.raises(AttributeError):
        ctrl.constellation.reconfigure({})
    classarr = getattr(ctrl.constellation, "Satellite")
    with pytest.raises(AttributeError):
        classarr.reconfigure({})
    sat = getattr(classarr, "mock_satellite")
    with pytest.raises(AttributeError):
        sat.reconfigure({})


def test_ctrl_reconfigure_call(mock_controller, mock_example_satellite):
    """Test reconfigure calls via satellite array."""
    ctrl, _ctx = mock_controller
    timeout = 2
    while timeout > 0 and len(ctrl.constellation.satellites) < 1:
        time.sleep(0.05)
        timeout -= 0.05
    assert len(ctrl.constellation.satellites) == 1, "Timed out while waiting for Satellite to be found"

    # cannot call reconfigure when in 'NEW'
    res = ctrl.constellation.MockExampleSatellite.reconfigure({})
    assert not res["MockExampleSatellite.mock_satellite"].success

    # prepare for reconfigure
    res = ctrl.constellation.MockExampleSatellite.mock_satellite.initialize({})
    assert res.msg == "transitioning"
    res = ctrl.constellation.MockExampleSatellite.mock_satellite.launch()
    assert res.msg == "transitioning"

    # sat array does not have reconfigure command
    with pytest.raises(AttributeError):
        ctrl.constellation.reconfigure({})
    # but class has
    res = ctrl.constellation.MockExampleSatellite.reconfigure({})
    assert res["MockExampleSatellite.mock_satellite"].msg == "transitioning"
    # and so does the satellite itself
    res = ctrl.constellation.MockExampleSatellite.mock_satellite.reconfigure({})
    assert res.msg == "transitioning"
    # test error handling:
    # This raises an exception (missing cfg dict as argument)
    with pytest.raises(RuntimeError):
        res = ctrl.constellation.MockExampleSatellite.reconfigure()


def test_satellite_hb_state(mock_controller, mock_satellite):
    """Test heartbeat state check."""
    satellite, _ctx = mock_satellite
    ctrl, _ctx = mock_controller
    timeout = 4
    while timeout > 0 and (len(ctrl.states) < 1 or len(ctrl.constellation.satellites) < 1):
        time.sleep(0.05)
        timeout -= 0.05
    assert len(ctrl.constellation.satellites) == 1, "Timed out while waiting for Satellite to be found"

    assert ctrl.states["Satellite.mock_satellite"] == SatelliteState.NEW
    assert ctrl.state_changes["Satellite.mock_satellite"] == satellite.fsm.last_changed


def test_satellite_init_w_fullcfg(mock_controller, mock_example_satellite, rawconfig):
    """Test cmd reception."""
    satellite, _ctx = mock_example_satellite
    ctrl, _ctx = mock_controller
    timeout = 2
    while timeout > 0 and len(ctrl.constellation.satellites) < 1:
        time.sleep(0.05)
        timeout -= 0.05
    assert len(ctrl.constellation.satellites) == 1, "Timed out while waiting for Satellite to be found"

    res = ctrl.constellation.MockExampleSatellite.mock_satellite.initialize(rawconfig)
    assert res.msg == "transitioning"
    time.sleep(0.1)
    assert satellite.config._config == flatten_config(rawconfig, "MockExampleSatellite", "mock_satellite")
    assert "current_limit" not in satellite.config._config, "Found unexpected item in Satellite cfg after init"
