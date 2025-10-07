"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2

This module provides network helper routines.
"""

import argparse
import socket

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


def get_interface_addresses(interface_names: list[str] | None) -> list[str]:
    """Get all multicast interface addresses for a given list of interface names."""
    interface_names = interface_names if interface_names is not None else get_interface_names()
    interface_addresses = []

    # Always add loopback interface
    interface_addresses.append("127.0.0.1")

    # Iterate over given names
    if_names = get_interface_names()
    for if_name in interface_names:
        if if_name in if_names:
            if_addr = get_addr(if_name)
            if if_addr is not None:
                interface_addresses.append(if_addr)

    # Remove duplicates without changing the order
    interface_addresses = list(dict.fromkeys(interface_addresses))

    return interface_addresses


def get_interface_names() -> list[str]:
    """Get all multicast interface names."""
    interface_names = []
    for if_name, if_stats in psutil.net_if_stats().items():
        if if_stats.isup and get_addr(if_name) is not None:
            interface_names.append(if_name)
    return interface_names


def validate_interface(interface_name: str) -> str:
    """Validate that the provided interface exists.

    interface :: name of an existing network interface.

    Raises ValueError if the interface does not exist.
    """
    for if_name in get_interface_names():
        if interface_name.lower() == if_name.lower():
            return if_name
    raise argparse.ArgumentTypeError(f"`{interface_name}` is not valid a network interface name.")
