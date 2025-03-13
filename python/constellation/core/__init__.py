"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2
"""

# Get version via project name in pyproject.toml
try:
    import importlib.metadata

    __version__ = importlib.metadata.version("ConstellationDAQ")
except:  # noqa: E722
    __version__ = "0"
    __version_code_name__ = "unknown"

# Get version code name via resource in constellation.core module
try:
    import importlib.resources

    __version_code_name__ = (importlib.resources.files("constellation.core") / "version_code_name").open().read().strip("\n")
except:  # noqa: E722
    __version_code_name__ = "unknown"
