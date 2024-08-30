#!/usr/bin/env python3
"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0

Provides the class for the Mariner example satellite
"""

from constellation.core.satellite import Satellite, SatelliteArgumentParser
import time
import logging
from typing import Any
from constellation.core.configuration import Configuration
from constellation.core.base import EPILOG


class CanopusStarTracker:
    def __init__(self, voltage, ampere, sample_period=0.1):
        self.voltage = voltage
        self.ampere = ampere
        self.sample_period = sample_period


class Mariner(Satellite):

    def do_initializing(self, config: Configuration) -> str:
        """Configure the Satellite and any associated hardware.

        The configuration is provided as Configuration object which works
        similar to a regular dictionary but tracks access to its keys. If a key
        does not exist or if one exists but is not used, the Satellite will
        automatically return an error or warning, respectively.

        """
        self.device = CanopusStarTracker(
            config["voltage"], config["current"], config["sample_period"]
        )
        return "Initialized"

    def do_run(self, payload: Any) -> str:
        """The main run routine.

        Here, the hardware is read out."""
        while not self._state_thread_evt.is_set():
            """
            Example work to be done while satellite is running
            """
            time.sleep(self.device.sample_period)
            print(f"New sample at {self.device.voltage} V")
        return "Finished acquisition."


# -------------------------------------------------------------------------


def main(args=None):
    """Start an example satellite.

    Provides a basic example satellite that can be controlled, and used as a
    basis for custom implementations.

    """
    import coloredlogs

    parser = SatelliteArgumentParser(description=main.__doc__, epilog=EPILOG)
    # this sets the defaults for our "demo" Satellite
    parser.set_defaults(name="Nine")
    # get a dict of the parsed arguments
    args = vars(parser.parse_args(args))

    # set up logging
    logger = logging.getLogger(args["name"])
    log_level = args.pop("log_level")
    coloredlogs.install(level=log_level.upper(), logger=logger)

    logger.info("Starting up satellite!")
    # start server with remaining args
    s = Mariner(**args)
    s.run_satellite()
