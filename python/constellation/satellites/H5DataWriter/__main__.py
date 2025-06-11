"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2

Provides the entry point for the H5DataWriter satellite
"""

from constellation.core.logging import setup_cli_logging
from constellation.core.satellite import SatelliteArgumentParser

from .H5DataWriter import H5DataWriter


def main(args=None):
    """Satellite receiving data and writing it to HDF5 files"""

    # Get a dict of the parsed arguments
    parser = SatelliteArgumentParser(description=main.__doc__)
    args = vars(parser.parse_args(args))

    # Set up logging
    setup_cli_logging(args.pop("level"))

    # Start satellite with remaining args
    s = H5DataWriter(**args)
    s.run_satellite()


if __name__ == "__main__":
    main()
