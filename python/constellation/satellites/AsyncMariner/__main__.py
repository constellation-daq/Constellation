"""
SPDX-FileCopyrightText: 2026 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2

Provides the entry point for the AsyncMariner example satellite
"""

import asyncio

from constellation.core import __version__
from constellation.core.async_experimental.async_satellite import SatelliteArgumentParser
from constellation.core.logging import setup_cli_logging

from .AsyncMariner import AsyncMariner


def main(args=None):
    """Async demonstrator satellite serving as prototype for new async satellites"""

    parser = SatelliteArgumentParser(description=main.__doc__, version=__version__)
    args = vars(parser.parse_args(args))

    setup_cli_logging(args.pop("level"))

    s = AsyncMariner(**args)
    asyncio.run(s.run_satellite())


if __name__ == "__main__":
    main()
