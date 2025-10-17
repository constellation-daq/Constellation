"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2

Module defining the Constellation communication protocol identifiers.
"""

from enum import StrEnum


class Protocol(StrEnum):
    """Identifier strings for the different protocols and versions."""

    CDTP1 = "CDTP\x01"
    CDTP2 = "CDTP\x02"
    CSCP1 = "CSCP\x01"
    CMDP1 = "CMDP\x01"
    CHP1 = "CHP\x01"
