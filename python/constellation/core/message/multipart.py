"""
SPDX-FileCopyrightText: 2025 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2

Provides message class for ZeroMQ multipart messages
"""

from collections.abc import Sequence
from io import BytesIO

import zmq


class MultipartMessage:
    def __init__(self, frames: Sequence[BytesIO | bytes]) -> None:
        # Convert streams to bytes
        self._frames = [self._get_bytes(frame) for frame in frames]

    @staticmethod
    def _get_bytes(data: BytesIO | bytes) -> bytes:
        if isinstance(data, BytesIO):
            return data.getvalue()
        return data

    @property
    def frames(self) -> list[bytes]:
        return self._frames

    def send(self, socket: zmq.Socket, send_flags: int = 0) -> None:  # type: ignore[type-arg]
        socket.send_multipart(self._frames, send_flags)
