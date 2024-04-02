#!/usr/bin/env python3
"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0
"""

import pytest
import time


# %%%%%%%%%%%%%%%
# TESTS
# %%%%%%%%%%%%%%%


@pytest.mark.forked
def test_controller_instantiation(mock_controller):
    """Test that we can create the controller."""
    assert mock_controller.name == "BaseController.mock_controller"


@pytest.mark.forked
def test_satellite_access_via_array(mock_controller, mock_satellite):
    """Test cmd reception."""
    time.sleep(0.1)
    res = mock_controller.constellation.Satellite.mock_satellite.get_state()
    assert res["msg"] == "new"
