"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2

Provides the entry point for the Keithley satellite
"""

from constellation.core.logging import setup_cli_logging
from constellation.core.satellite import SatelliteArgumentParser

from .Keithley import Keithley


def main(args=None):
    """Satellite controlling a Keithley power supply"""

    # Get a dict of the parsed arguments
    parser = SatelliteArgumentParser(description=main.__doc__)
    args = vars(parser.parse_args(args))

    # Set up logging
    setup_cli_logging(args.pop("level"))

    # Start satellite with remaining args
    s = Keithley(**args)
    s.run_satellite()


if __name__ == "__main__":
    main()
