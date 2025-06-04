"""
SPDX-FileCopyrightText: 2025 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2
"""

from constellation.core.logging import setup_cli_logging
from constellation.core.transmitter_satellite import TransmitterSatelliteArgumentParser

from .PyRandomTransmitter import PyRandomTransmitter


def main(args=None):
    # Get a dict of the parsed arguments
    parser = TransmitterSatelliteArgumentParser(description=main.__doc__)
    args = vars(parser.parse_args(args))

    # Set up logging
    setup_cli_logging(args.pop("level"))

    # Start satellite with remaining args
    s = PyRandomTransmitter(**args)
    s.run_satellite()


if __name__ == "__main__":
    main()
