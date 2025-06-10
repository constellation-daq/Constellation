"""
SPDX-FileCopyrightText: 2025 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2
"""

from argparse import ArgumentTypeError

import pytest

from constellation.core.network import (
    get_interface_addresses,
    validate_interface,
)


@pytest.mark.forked
def test_interface_addr():
    """Test the retrieval of network interfaces."""
    assert get_interface_addresses("*")
    assert get_interface_addresses("127.0.0.1")


@pytest.mark.forked
def test_interface_validation():
    """Test the validation of network interfaces."""
    assert validate_interface("*") == "*"
    assert validate_interface("127.0.0.1") == "127.0.0.1"
    with pytest.raises(ArgumentTypeError):
        validate_interface("non_existing_interface_name")
