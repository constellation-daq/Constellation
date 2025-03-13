"""
SPDX-FileCopyrightText: 2025 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2

This module provides a base class for Constellation Satellite modules.
"""

import logging
from typing import Any

import coloredlogs  # type: ignore[import-untyped]

from .cmdp import CMDPTransmitter


class ConstellationLogger(logging.getLoggerClass()):  # type: ignore[misc]
    """Custom Logger class for Constellation."""

    def __init__(self, *args: Any, **kwargs: Any):
        """Init logger."""
        super().__init__(*args, **kwargs)

    def trace(self, msg: str, *args: Any, **kwargs: Any) -> None:
        """Define level for verbose information which allows to follow the call
        stack of the host program."""
        self.log(logging.TRACE, msg, *args, **kwargs)  # type: ignore[attr-defined]

    def status(self, msg: str, *args: Any, **kwargs: Any) -> None:
        """Define level for important information about the host program to the
        end user with low frequency."""
        self.log(logging.STATUS, msg, *args, **kwargs)  # type: ignore[attr-defined]

    def error(self, msg: str, *args: Any, **kwargs: Any) -> None:
        """Map error level to CRITICAL."""
        self.log(logging.CRITICAL, msg, *args, **kwargs)


class ZeroMQSocketLogHandler(logging.Handler):
    """This handler sends records to a ZMQ socket."""

    def __init__(self, transmitter: CMDPTransmitter):
        super().__init__()
        self.transmitter = transmitter

    def emit(self, record: logging.LogRecord) -> None:
        self.transmitter.send_log(record)

    def close(self) -> None:
        if not self.transmitter.closed():
            self.transmitter.close()


def setup_cli_logging(level: str) -> None:
    """
    Sets up the CLI logging configuration.

    Defines the following log levels:

    - logging.NOTSET : 0
    - logging.TRACE : 5
    - logging.DEBUG : 10
    - logging.INFO : 20
    - logging.WARNING : 30
    - logging.STATUS : 35
    - logging.ERROR : mapped to CRITICAL
    - logging.CRITICAL : 50
    """
    # Set default logger class
    logging.setLoggerClass(ConstellationLogger)
    # Add custom log levels
    logging.TRACE = logging.DEBUG - 5  # type: ignore[attr-defined]
    logging.addLevelName(logging.TRACE, "TRACE")  # type: ignore[attr-defined]
    logging.STATUS = logging.WARNING + 5  # type: ignore[attr-defined]
    logging.addLevelName(logging.STATUS, "STATUS")  # type: ignore[attr-defined]
    # Set default log level
    coloredlogs.DEFAULT_LOG_LEVEL = logging.getLevelNamesMapping()[level.upper()]
