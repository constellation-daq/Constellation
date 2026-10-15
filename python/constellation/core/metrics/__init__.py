"""
SPDX-FileCopyrightText: 2026 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2

DQM extension types for structured CMDP metric data.
"""

from .histogram import Histogram1D, Histogram2D
from .matrix import DType, Matrix

__all__ = ["DType", "Matrix", "Histogram1D", "Histogram2D"]
