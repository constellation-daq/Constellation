"""
SPDX-FileCopyrightText: 2026 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2

Module containing definitions for CSCP.
"""

import re
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


def states_except(disallowed_states: list[SatelliteState]) -> list[SatelliteState]:
    """Get list of all states except states from a given list"""
    allowed_states: list[SatelliteState] = []
    for enum_entry in SatelliteState:
        if enum_entry not in disallowed_states:
            allowed_states.append(enum_entry)
    return allowed_states


def is_valid_satellite_name(satellite_name: str) -> bool:
    """Checks if a satellite name is valid

    A satellite name may contain alphanumeric characters and underscores and may not be empty."""
    return bool(re.match(r"^\w+$", satellite_name))


def is_valid_satellite_type(satellite_type: str) -> bool:
    """Checks if a satellite type is valid

    A satellite type may contain alphanumeric characters and underscores and may not be empty."""
    return bool(re.match(r"^\w+$", satellite_type))


def is_valid_canonical_name(canonical_name: str) -> bool:
    """Checks if a canonical name is valid

    A canonical name consists of two parts, separated by a period.
    Both parts may contain alphanumeric characters and underscores and may not be empty.
    """
    name_parts = canonical_name.split(".")
    if len(name_parts) != 2:
        return False
    return is_valid_satellite_type(name_parts[0]) and is_valid_satellite_type(name_parts[1])
