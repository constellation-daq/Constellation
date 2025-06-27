"""
SPDX-FileCopyrightText: 2025 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2
"""

from argparse import ArgumentTypeError

import pytest

from constellation.core.network import (
    get_interface_addresses,
    get_interface_names,
    get_loopback_interface_name,
    validate_interface,
)


def test_interface_addr():
    """Test the retrieval of network interface addresses."""
    assert "127.0.0.1" in get_interface_addresses([get_loopback_interface_name()])


def test_interface_names():
    """Test the retrieval of network interface names."""
    assert get_loopback_interface_name() in get_interface_names()


def test_interface_validation():
    """Test the validation of network interfaces."""
    lo = get_loopback_interface_name()
    assert validate_interface(lo) == lo
    with pytest.raises(ArgumentTypeError):
        validate_interface("non_existing_interface_name")
