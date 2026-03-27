"""
SPDX-FileCopyrightText: 2026 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2

Module containing definitions for CSCP.
"""

from enum import Enum


class SatelliteState(Enum):
    """Available states to cycle through."""

    # Idle state without any configuration
    NEW = 0x10
    # Initialized state with configuration but not (fully) applied
    INIT = 0x20
    # Prepared state where configuration is applied
    ORBIT = 0x30
    # Running state where DAQ is running
    RUN = 0x40
    # Safe fallback state if error is discovered during run
    SAFE = 0xE0
    # Error state if something went wrong
    ERROR = 0xF0
    #
    #  TRANSITIONAL STATES
    #
    initializing = 0x12
    launching = 0x23
    landing = 0x32
    reconfiguring = 0x33
    starting = 0x34
    stopping = 0x43
    interrupting = 0x0E
    # state if shutdown
    DEAD = 0xFF

    def transitions_to(self, state: Enum) -> bool:
        # Target steady state indicated by the lower four bits
        return bool(((self.value & 0x0F) << 4) == state.value)
