"""
SPDX-FileCopyrightText: 2025 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2

Module containing definitions for CMDP.
"""

import logging
from dataclasses import dataclass
from enum import IntEnum


class LogLevel(IntEnum):
    """Constellation log levels"""

    TRACE = logging.DEBUG - 5
    DEBUG = logging.DEBUG
    INFO = logging.INFO
    WARNING = logging.WARNING
    STATUS = logging.WARNING + 5
    CRITICAL = logging.CRITICAL


def log_level_from_levelno(levelno: int) -> LogLevel:
    retval = LogLevel.CRITICAL
    for level in LogLevel:
        if levelno >= level.value:
            retval = level
        else:
            # Special case: map ERROR to CRITICAL
            if levelno >= logging.ERROR:
                retval = LogLevel.CRITICAL
            break
    return retval


@dataclass
class Metric:
    name: str
    unit: str
    description: str
