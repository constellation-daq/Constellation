"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2

Provides the entry point for the LeCroy/LeCrunch satellite
"""

from constellation.core.datasender import DataSenderArgumentParser
from constellation.core.logging import setup_cli_logging

from .LeCroySatellite import LeCroySatellite


def main(args=None) -> None:
    """Satellite controlling a LeCroy oscilloscope"""

    # Get a dict of the parsed arguments
    parser = DataSenderArgumentParser(description=main.__doc__)
    args = vars(parser.parse_args(args))

    # Set up logging
    setup_cli_logging(args.pop("level"))

    # Start satellite with remaining args
    s = LeCroySatellite(**args)
    s.run_satellite()


if __name__ == "__main__":
    main()
