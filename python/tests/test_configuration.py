#!/usr/bin/env python3
"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0
"""

import threading
import time
from unittest.mock import MagicMock, patch

import pytest
from conftest import mocket
from constellation.confighandler import Configuration, get_config
from constellation.cscp import CSCPMessageVerb
from constellation.satellite import Satellite


@pytest.fixture
def config():
    """Fixture for specific configuration"""
    config = {}
    for category in ["constellation", "satellites"]:
        config.update(
            get_config(
                "python/constellation/configs/example.toml",
                category,
                "example_satellite",
                "example_device2",
            )
        )
    yield Configuration(config)


@pytest.fixture
def mock_device_satellite(mock_chirp_socket):
    """Mock a Satellite for a specific device, ie. a class inheriting from Satellite."""

    def mocket_factory(*args, **kwargs):
        m = mocket()
        return m

    class MockDeviceSatellite(Satellite):
        def do_initializing(self, payload):
            self.voltage = self.config.setdefault("voltage", 10)
            return "finished with mock initialization"

    with patch("constellation.base.zmq.Context") as mock:
        mock_context = MagicMock()
        mock_context.socket = mocket_factory
        mock.return_value = mock_context
        s = MockDeviceSatellite("mock_satellite", "mockstellation", 11111, 22222, 33333)
        t = threading.Thread(target=s.run_satellite)
        t.start()
        # give the threads a chance to start
        time.sleep(0.1)
        yield s


def test_unused_values(config):
    assert config.has_unused_values()
    config.setdefault("voltage")
    config.setdefault("ampere", default=10)
    assert config.has_unused_values()

    for key in config.get_keys():
        config.setdefault(key)

    assert not config.has_unused_values()


def test_sending_config(config, mock_device_satellite, mock_cmd_transmitter):
    satellite = mock_device_satellite
    sender = mock_cmd_transmitter

    sender.send_request("initialize", config._config)
    time.sleep(0.1)
    req = sender.get_message()
    assert "transitioning" in req.msg.lower()
    assert req.msg_verb == CSCPMessageVerb.SUCCESS

    assert satellite.config._config == config._config
