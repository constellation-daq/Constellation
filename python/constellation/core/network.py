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


def get_interface_addresses(interface: str = "*") -> set[str]:
    """Get all multicast interfaces."""
    interface_addresses = []
    for if_idx, if_name in socket.if_nameindex():
        if_addr = get_addr(if_name)
        if interface == "*" or if_name == interface or if_addr == interface:
            if if_addr is not None:
                interface_addresses.append(if_addr)
    return set(interface_addresses)


def get_interface_addresses_args() -> list[str]:
    """Get all multicast interfaces and placeholder."""
    interface_addresses = ["*"]
    for if_idx, if_name in socket.if_nameindex():
        if_addr = get_addr(if_name)
        if if_addr is not None:
            interface_addresses.append(if_addr)
    return interface_addresses


def validate_interface(interface: str) -> str:
    """Validate that the provided interface exists.

    interface :: IPv4 address or name of an existing network interface.

    Returns IPv4 address of interface or '*' for any/all interfaces.

    Raises ValueError if the interface/IP does not exist.
    """
    # if using any/all interfaces:
    if interface == "*":
        return interface

    for if_idx, if_name in socket.if_nameindex():
        if_addr = get_addr(if_name)
        if if_name == interface or if_addr == interface:
            if if_addr is not None:
                return if_addr
    raise argparse.ArgumentTypeError(
        f"'{interface}' is neither an network interface name nor an interface's IPv4 ip address."
    )
