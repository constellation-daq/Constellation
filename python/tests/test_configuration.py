"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2
"""

import time

from constellation.core.configuration import flatten_config
from constellation.core.message.cscp1 import CSCP1Message


def test_unused_values(config):
    assert config.has_unused_values()
    config.setdefault("voltage")
    config.setdefault("ampere", default=10)
    assert config.has_unused_values()

    for key in config.get_keys():
        config.setdefault(key)

    assert not config.has_unused_values()


def test_sending_config(config, mock_example_satellite, mock_cmd_transmitter):
    satellite, _ctx = mock_example_satellite
    sender = mock_cmd_transmitter

    sender.send_request("initialize", config._config)
    time.sleep(0.1)
    req = sender.get_message()
    assert isinstance(req, CSCP1Message)
    assert "transitioning" in str(req.verb_msg).lower()
    assert req.verb_type == CSCP1Message.Type.SUCCESS
    assert "mode" in satellite.config._config, "Default value added by Satellite not found"
    satellite.config._config.pop("mode")
    assert satellite.config._config == config._config


def test_config_flattening(rawconfig):
    """Test that config flattening works"""
    assert "ampere" in flatten_config(rawconfig, "mocksat"), "Parameter missing from class part of config"
    assert "_role" in flatten_config(rawconfig, "mocksat"), "Parameter missing from constellation part of cfg"
    val = flatten_config(rawconfig, "mocksat", "device2")["_role"]
    assert val == "essential", "Parameter incorrect from constellation part of cfg"
    val = flatten_config(rawconfig, "mocksat", "device1")["voltage"]
    assert val == 5000, "Error flattening cfg for Satellite"
    val = flatten_config(rawconfig, "mocksat", "device2")["voltage"]
    assert val == 3000, "Error flattening cfg for Satellite"
    assert flatten_config(rawconfig, "nonsatellite"), "Missing dict for non-existing class"
    assert "verbosity" in flatten_config(rawconfig, "nonsatellite"), "Missing value for non-existing class"
    assert flatten_config(rawconfig, "mocksat", "device3"), "Missing dict for not explicitly-defined sat"
