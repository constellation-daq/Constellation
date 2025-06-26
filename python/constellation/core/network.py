"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2

This module provides network helper routines.
"""

import argparse
import socket
from typing import Optional

import psutil


def get_addr(if_name: str) -> str | None:
    """Get the IPv4 address for the given interface name."""
    if_addrs = psutil.net_if_addrs().get(if_name) or []
    for if_addr in if_addrs:
        if if_addr.family == socket.AF_INET:
            return str(if_addr.address)
    return None


def get_loopback_interface_name() -> str:
    for if_idx, if_name in socket.if_nameindex():
        if get_addr(if_name) == "127.0.0.1":
            return if_name
    return "lo"


def get_interface_addresses(interface_names: Optional[list[str]]) -> set[str]:
    """Get all multicast interface addresses for a given list of interface names."""
    interface_names = interface_names if interface_names is not None else get_interface_names()
    interface_addresses = []
    for if_idx, if_name in socket.if_nameindex():
        if_addr = get_addr(if_name)
        if if_addr is not None and if_name in interface_names:
            interface_addresses.append(if_addr)
    return set(interface_addresses)


def get_interface_names() -> list[str]:
    """Get all multicast interface names."""
    interface_names = []
    for if_idx, if_name in socket.if_nameindex():
        if_addr = get_addr(if_name)
        if if_addr is not None:
            interface_names.append(if_name)
    return interface_names


def validate_interface(interface: str) -> str:
    """Validate that the provided interface exists.

    interface :: IPv4 address or name of an existing network interface.

    Returns IPv4 address of interface or '*' for any/all interfaces.

    Raises ValueError if the interface/IP does not exist.
    """
    for if_idx, if_name in socket.if_nameindex():
        if_addr = get_addr(if_name)
        if if_addr is not None and if_name == interface:
            return if_name
    raise argparse.ArgumentTypeError(f"`{interface}` is not valid a network interface name.")
