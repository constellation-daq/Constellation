"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2

Provides the entry point for the LeCroy/LeCrunch satellite
"""

from constellation.core.base import EPILOG
from constellation.core.logging import setup_cli_logging
from constellation.core.datasender import DataSenderArgumentParser

from .LeCrunchSatellite import LeCrunchSatellite

from typing import Any


def main(args: Any = None) -> None:
    """Satellite controlling a LeCroy oscilloscope"""

    # Get a dict of the parsed arguments
    parser = DataSenderArgumentParser(description=main.__doc__, epilog=EPILOG)
    args = vars(parser.parse_args(args))

    # Set up logging
    setup_cli_logging(args.pop("log_level"))

    # Start satellite with remaining args
    s = LeCrunchSatellite(**args)
    s.run_satellite()


if __name__ == "__main__":
    main()

