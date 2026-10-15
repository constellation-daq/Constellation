"""
SPDX-FileCopyrightText: 2026 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2

Histogram types for CMDP metrics.
"""

import struct

import msgpack
import numpy as np

from .matrix import _DTYPE_TO_NUMPY, _NUMPY_TO_DTYPE, EXT_TYPE_MATRIX, DType, Matrix

# MsgPack ext type codes
EXT_TYPE_HISTOGRAM1D = 0x02
EXT_TYPE_HISTOGRAM2D = 0x03


class Histogram1D:
    """1D histogram with uniform binning and typed bin counts."""

    def __init__(self, start: float, end: float, bins: np.ndarray) -> None:
        """Construct from bin edges and a 1D array of bin counts."""
        if bins.ndim != 1:
            raise ValueError(f"Histogram1D requires 1D bin array, got {bins.ndim}D")
        self._start = float(start)
        self._end = float(end)

        le_dtype = bins.dtype.newbyteorder("<")
        if le_dtype not in _NUMPY_TO_DTYPE:
            # Default to float64 for unsupported dtypes
            le_dtype = np.dtype("<f8")
        self._dtype = _NUMPY_TO_DTYPE[le_dtype]
        self._bins = np.ascontiguousarray(bins, dtype=le_dtype)

    @property
    def start(self) -> float:
        return self._start

    @property
    def end(self) -> float:
        return self._end

    @property
    def dtype(self) -> DType:
        return self._dtype

    @property
    def nbins(self) -> int:
        return len(self._bins)

    @property
    def bins(self) -> np.ndarray:
        return self._bins

    def to_ext_bytes(self) -> bytes:
        """Serialize to ext type payload bytes."""
        header = struct.pack("<ddBH", self._start, self._end, int(self._dtype), self.nbins)
        return header + self._bins.tobytes()

    def to_msgpack_ext(self) -> msgpack.ExtType:
        """Wrap as MsgPack ExtType for packing."""
        return msgpack.ExtType(EXT_TYPE_HISTOGRAM1D, self.to_ext_bytes())

    @classmethod
    def from_ext_bytes(cls, raw: bytes) -> "Histogram1D":
        """Deserialize from ext payload bytes."""
        if len(raw) < 19:
            raise ValueError("Histogram1D ext payload too small")

        start, end, dtype_tag, nbins = struct.unpack_from("<ddBH", raw, 0)
        dtype = DType(dtype_tag)
        np_dtype = _DTYPE_TO_NUMPY[dtype]
        elem_size = np_dtype.itemsize

        expected_size = 19 + nbins * elem_size
        if len(raw) != expected_size:
            raise ValueError(f"Histogram1D data size mismatch: expected {expected_size}, got {len(raw)}")

        bins = np.frombuffer(raw, dtype=np_dtype, offset=19, count=nbins).copy()
        result = object.__new__(cls)
        result._start = start
        result._end = end
        result._dtype = dtype
        result._bins = bins
        return result

    def __repr__(self) -> str:
        return f"Histogram1D({self.nbins} bins, {self._dtype.name}, [{self._start}, {self._end}])"


class Histogram2D:
    """2D histogram with uniform x and y binning, backed by a Matrix."""

    def __init__(
        self,
        x_start: float,
        x_end: float,
        y_start: float,
        y_end: float,
        matrix: Matrix,
    ) -> None:
        """Construct from x/y bin edges and a bin count Matrix."""
        self._x_start = float(x_start)
        self._x_end = float(x_end)
        self._y_start = float(y_start)
        self._y_end = float(y_end)
        self._matrix = matrix

    @property
    def x_start(self) -> float:
        return self._x_start

    @property
    def x_end(self) -> float:
        return self._x_end

    @property
    def y_start(self) -> float:
        return self._y_start

    @property
    def y_end(self) -> float:
        return self._y_end

    @property
    def matrix(self) -> Matrix:
        return self._matrix

    def to_ext_bytes(self) -> bytes:
        """Serialize to ext type payload bytes."""
        header = struct.pack("<dddd", self._x_start, self._x_end, self._y_start, self._y_end)
        # Embed the matrix as a complete MsgPack ext object
        matrix_ext = msgpack.packb(self._matrix.to_msgpack_ext())
        return header + matrix_ext

    def to_msgpack_ext(self) -> msgpack.ExtType:
        """Wrap as MsgPack ExtType for packing."""
        return msgpack.ExtType(EXT_TYPE_HISTOGRAM2D, self.to_ext_bytes())

    @classmethod
    def from_ext_bytes(cls, raw: bytes) -> "Histogram2D":
        """Deserialize from ext payload bytes."""
        if len(raw) < 32:
            raise ValueError("Histogram2D ext payload too small")

        x_start, x_end, y_start, y_end = struct.unpack_from("<dddd", raw, 0)

        # Unpack the embedded matrix MsgPack ext object
        matrix_data = raw[32:]
        matrix_obj = msgpack.unpackb(
            matrix_data,
            ext_hook=lambda code, data: (
                Matrix.from_ext_bytes(data) if code == EXT_TYPE_MATRIX else msgpack.ExtType(code, data)
            ),
            raw=False,
        )
        if not isinstance(matrix_obj, Matrix):
            raise ValueError("Failed to unpack embedded Matrix from Histogram2D")

        result = object.__new__(cls)
        result._x_start = x_start
        result._x_end = x_end
        result._y_start = y_start
        result._y_end = y_end
        result._matrix = matrix_obj
        return result

    def __repr__(self) -> str:
        return (
            f"Histogram2D({self._matrix.rows}x{self._matrix.cols}, "
            f"x=[{self._x_start}, {self._x_end}], "
            f"y=[{self._y_start}, {self._y_end}])"
        )
