"""
SPDX-FileCopyrightText: 2026 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2

Dense 2D matrix with typed elements for CMDP metrics.
"""

import enum
import struct

import msgpack
import numpy as np

# MsgPack ext type code
EXT_TYPE_MATRIX = 0x01


class DType(enum.IntEnum):
    """Element data types for metric payloads."""

    UINT32 = 0
    UINT64 = 1
    FLOAT32 = 2
    FLOAT64 = 3


# Map DType tags to NumPy dtype strings
_DTYPE_TO_NUMPY = {
    DType.UINT32: np.dtype("<u4"),
    DType.UINT64: np.dtype("<u8"),
    DType.FLOAT32: np.dtype("<f4"),
    DType.FLOAT64: np.dtype("<f8"),
}

# Map NumPy dtype to DType tags
_NUMPY_TO_DTYPE = {v: k for k, v in _DTYPE_TO_NUMPY.items()}


class Matrix:
    """Dense 2D matrix backed by a NumPy array."""

    def __init__(self, data: np.ndarray) -> None:
        """Construct from a 2D NumPy array with a supported dtype."""
        if data.ndim != 2:
            raise ValueError(f"Matrix requires 2D array, got {data.ndim}D")

        # Ensure little-endian
        le_dtype = data.dtype.newbyteorder("<")
        if le_dtype not in _NUMPY_TO_DTYPE:
            raise ValueError(f"Unsupported dtype {data.dtype}, must be one of uint32, uint64, float32, float64")

        self._data = np.ascontiguousarray(data, dtype=le_dtype)
        self._dtype = _NUMPY_TO_DTYPE[le_dtype]

    @property
    def rows(self) -> int:
        return self._data.shape[0]

    @property
    def cols(self) -> int:
        return self._data.shape[1]

    @property
    def dtype(self) -> DType:
        return self._dtype

    @property
    def data(self) -> np.ndarray:
        return self._data

    def to_ext_bytes(self) -> bytes:
        """Serialize to ext type payload bytes."""
        header = struct.pack("<BHH", self._dtype.value, self.rows, self.cols)
        return header + self._data.tobytes()

    def to_msgpack_ext(self) -> msgpack.ExtType:
        """Wrap as MsgPack ExtType for packing."""
        return msgpack.ExtType(EXT_TYPE_MATRIX, self.to_ext_bytes())

    @classmethod
    def from_ext_bytes(cls, raw: bytes) -> "Matrix":
        """Deserialize from ext payload bytes."""
        if len(raw) < 5:
            raise ValueError("Matrix ext payload too small")

        dtype_tag, rows, cols = struct.unpack_from("<BHH", raw, 0)
        dtype = DType(dtype_tag)
        np_dtype = _DTYPE_TO_NUMPY[dtype]

        expected_size = 5 + rows * cols * np_dtype.itemsize
        if len(raw) != expected_size:
            raise ValueError(f"Matrix data size mismatch: expected {expected_size}, got {len(raw)}")

        data = np.frombuffer(raw, dtype=np_dtype, offset=5).reshape(rows, cols).copy()
        result = object.__new__(cls)
        result._data = data
        result._dtype = dtype
        return result

    def __repr__(self) -> str:
        return f"Matrix({self.rows}x{self.cols}, {self._dtype.name.lower()})"
