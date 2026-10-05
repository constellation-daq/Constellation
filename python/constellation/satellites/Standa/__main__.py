"""
SPDX-FileCopyrightText: 2026 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2

Provides the entry point for the Standa motor controller satellite
"""

from constellation.core.logging import setup_cli_logging
from constellation.core.satellite import SatelliteArgumentParser

from .Standa import Standa


def main(args=None):
    """Satellite controlling a Standa motor controller"""

    parser = SatelliteArgumentParser(description=main.__doc__)
    args = vars(parser.parse_args(args))

    # Set up logging
    setup_cli_logging(args.pop("level"))

    # start server with remaining args
    s = Standa(**args)
    s.run_satellite()


if __name__ == "__main__":
    main()
