"""
SPDX-FileCopyrightText: 2025 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2

This module provides a base class for Constellation Satellite modules.
"""

import logging
from typing import Any

from rich.console import Console, ConsoleRenderable
from rich.logging import RichHandler
from rich.theme import Theme

from .cmdp import CMDPPublisher

# Defines the following log levels:
#
# - `logging.NOTSET`   :  0
# - `logging.TRACE`    :  5
# - `logging.DEBUG`    : 10
# - `logging.INFO`     : 20
# - `logging.WARNING`  : 30
# - `logging.STATUS`   : 35
# - `logging.ERROR`    : mapped to CRITICAL
# - `logging.CRITICAL` : 50

# Custom log levels
logging.TRACE = logging.DEBUG - 5  # type: ignore[attr-defined]
logging.STATUS = logging.WARNING + 5  # type: ignore[attr-defined]
# Register log levels
logging.addLevelName(logging.TRACE, "TRACE")  # type: ignore[attr-defined]
logging.addLevelName(logging.STATUS, "STATUS")  # type: ignore[attr-defined]
logging.addLevelName(logging.ERROR, "CRITICAL")


class ConstellationLogger(logging.Logger):
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

    def error(self, msg: str, *args: Any, **kwargs: Any) -> None:  # type: ignore[override]
        """Map error level to CRITICAL."""
        self.log(logging.CRITICAL, msg, *args, **kwargs)


class ZeroMQSocketLogHandler(logging.Handler):
    """This handler sends records to a ZMQ socket."""

    def __init__(self, transmitter: CMDPPublisher):
        super().__init__()
        self.transmitter = transmitter

    def emit(self, record: logging.LogRecord) -> None:
        if self.transmitter.has_log_subscribers(record):
            self.transmitter.send_log(record)

    def close(self) -> None:
        if not self.transmitter.closed():
            self.transmitter.close()


class ConstellationRichHandler(RichHandler):
    def render_message(self, record: logging.LogRecord, message: str) -> ConsoleRenderable:
        """Render message text"""
        tb: str | None = getattr(record, "traceback", None)
        if tb:
            message += "\n" + tb
        return super().render_message(record, message)


def setup_cli_logging(level: str) -> None:
    """Sets up the CLI logging configuration"""
    # Get log level integer
    levelno = logging.getLevelNamesMapping()[level.upper()]
    # Logging format
    format = "[%(name)s] %(message)s"
    console_theme = Theme(
        {
            "logging.level.trace": "gray62",
            "logging.level.debug": "cyan",
            "logging.level.info": "bold cyan",
            "logging.level.warning": "bold yellow",
            "logging.level.status": "bold green",
            "logging.level.error": "bold red",
            "logging.level.critical": "bold red",
        }
    )
    handler = ConstellationRichHandler(
        level=levelno,
        console=Console(theme=console_theme),
        show_path=False,
        show_time=True,
        omit_repeated_times=False,
        log_time_format="|%Y-%m-%d %H:%M:%S|",
    )
    # Logging configuration
    logging.basicConfig(format=format, handlers=[handler])
    # Set ConstellationLogger as default logging class
    logging.setLoggerClass(ConstellationLogger)
