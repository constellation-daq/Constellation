"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2

This is the entry point for the keithley satellite.
"""

from constellation.core.logging import setup_cli_logging
from constellation.core.satellite import SatelliteArgumentParser

from .LakeShore218 import LakeShore218


def main(args=None):
    """Satellite controlling a LakeShore Model 218 temperature monitor"""

    parser = SatelliteArgumentParser(description=main.__doc__)
    args = vars(parser.parse_args(args))

    # Set up logging
    setup_cli_logging(args.pop("level"))

    # start server with remaining args
    s = LakeShore218(**args)
    s.run_satellite()


if __name__ == "__main__":
    main()
