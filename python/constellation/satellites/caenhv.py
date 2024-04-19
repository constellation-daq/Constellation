#!/usr/bin/env python3
"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0

This module provides the class for a Constellation Satellite.
"""
import logging
import argparse
import coloredlogs

from ..core.satellite import Satellite
from ..core.commandmanager import cscp_requestable
import pycaenhv


class CaenHvSatellite(Satellite):

    def do_initializing(self, configuration):
        """Set up connection to HV module and configure settings."""
        if getattr(self, "caen", None):
            # old connection
            self.caen.disconnect()
        # SY5527 and similar: pycaenhv
        self.caen = pycaenhv.CaenHVModule()
        system = configuration["system"]
        user = configuration["username"]
        pw = configuration["password"]
        link = configuration["link"]
        link_arg = configuration["link_argument"]
        self.caen.connect(system, link, link_arg, user, pw)
        if not self.caen.is_connected():
            raise RuntimeError("No connection to Caen HV crate established")

        # process configuration
        with self.caen as crate:
            for brdno, brd in crate.boards.items():
                # loop over boards
                self.log.info("Configuring board %s", brd)
                for chno, ch in enumerate(brd.channels):
                    # loop over channels
                    for par in ch.parameter_names:
                        # loop over parameters
                        # construct configuration key
                        key = f"board{brdno}_ch{chno}_{par.lower()}"
                        self.log.debug("Checking configuration for key '%s'", key)
                        try:
                            # retrieve and set value
                            val = configuration[key]
                            if par in ["Pw"]:
                                # do not want to power up just yet
                                continue
                            # the board essentially only knows 'float'-type
                            # arguments except for 'Pw':
                            val = float(val)
                            setattr(ch, par, val)
                            self.log.debug(
                                "Configuring %s on board %s, ch %s with value '%s'",
                                par,
                                brdno,
                                chno,
                                val,
                            )
                        except KeyError:
                            # nothing in the cfg, leave as it is
                            pass
                        except ValueError as e:
                            raise RuntimeError(
                                f"Error in configuration for key {key}: {repr(e)}"
                            )

        return "Connected and configured"

    def do_launching(self, payload):
        # TODO implement
        return "Launched"

    def do_interrupting(self, payload):
        # TODO implement
        return "Interrupted"

    def fail_gracefully(self):
        if getattr(self, "caen", None):
            self.caen.disconnect()
        return "error handled"

    def get_channel_value(self, board: int, channel: int, par: str):
        # TODO implement metric helper method
        val = 42
        return val

    @cscp_requestable
    def get_parameter(self, request):
        """Return the value of a parameter.

        Payload: dictionary with 'board', 'channel' and 'parameter' keys
        providing the respective values.

        """
        board = request.payload["board"]
        chno = int(request.payload["channel"])
        par = request.payload["parameter"]
        with self.caen as crate:
            val = crate.boards[board].channels[chno].parameters[par]
        return val, None, None

    def about(self):
        # TODO implement cmd
        pass


# ---


def main(args=None):
    """Start the base Satellite server."""
    parser = argparse.ArgumentParser(description=main.__doc__)
    parser.add_argument("--log-level", default="info")
    parser.add_argument("--cmd-port", type=int, default=23999)
    parser.add_argument("--mon-port", type=int, default=55556)
    parser.add_argument("--hb-port", type=int, default=61234)
    parser.add_argument("--interface", type=str, default="*")
    parser.add_argument("--name", type=str, default="CaenHVModule")
    parser.add_argument("--group", type=str, default="constellation")
    args = parser.parse_args(args)

    # set up logging
    logger = logging.getLogger(args.name)
    coloredlogs.install(level=args.log_level.upper(), logger=logger)

    logger.info("Starting up satellite!")
    # start server with remaining args
    s = CaenHvSatellite(
        name=args.name,
        group=args.group,
        cmd_port=args.cmd_port,
        hb_port=args.hb_port,
        mon_port=args.mon_port,
        interface=args.interface,
    )
    s.run_satellite()


if __name__ == "__main__":
    main()
