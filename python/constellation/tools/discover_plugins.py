"""
SPDX-FileCopyrightText: 2026 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2
"""

import importlib
import importlib.metadata
import pkgutil


def discover_plugins(namespace: str) -> dict[str, str]:
    plugins: dict[str, str] = {}

    # From module namespace
    module = importlib.import_module(namespace)
    plugins.update(
        {name.split(".")[-1]: name for finder, name, ispkg in pkgutil.iter_modules(module.__path__, module.__name__ + ".")}
    )

    # From entry points
    entry_points = importlib.metadata.entry_points(group=namespace)
    for entry_point in entry_points:
        plugins[entry_point.name] = entry_point.value

    return plugins


if __name__ == "__main__":
    satellites = discover_plugins("constellation.satellites")
    for satellite_type, module in satellites.items():
        print(f"{satellite_type} found in {module}")
