"""
SPDX-FileCopyrightText: 2025 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2

Provides message class for ZeroMQ multipart messages
"""

from io import BytesIO

import zmq


class MultipartMessage:
    def __init__(self, streams: list[BytesIO]) -> None:
        # Convert stream to bytes object
        self._frames = [stream.getvalue() for stream in streams]

    @property
    def frames(self) -> list[bytes]:
        return self._frames

    def send(self, socket: zmq.Socket, send_flags: int = 0) -> None:  # type: ignore[type-arg]
        socket.send_multipart(self._frames, send_flags)
