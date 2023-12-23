"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0
"""

# Try to get version from installation
try:
    import importlib.metadata
    __version__ = importlib.metadata.version('pyconstellation') # module name
except:
    __version__ = 'unknown'
