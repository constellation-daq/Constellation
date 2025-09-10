"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2

Module implementing the Constellation Satellite Control Protocol.
"""

from threading import Lock
from typing import Any

import zmq

from .message.cscp1 import CSCP1Message


class CommandTransmitter:
    """Class implementing Constellation Satellite Control Protocol."""

    def __init__(self, name: str, socket: zmq.Socket):  # type: ignore[type-arg]
        self._name = name
        self._socket = socket
        self._lock = Lock()

    def send_request(self, command: str, payload: Any = None, tags: dict[str, Any] | None = None) -> None:
        """Send a command request to a Satellite with an optional payload."""
        self._dispatch((CSCP1Message.Type.REQUEST, command), payload, tags, zmq.NOBLOCK)

    def request_get_response(self, command: str, payload: Any = None, tags: dict[str, Any] | None = None) -> CSCP1Message:
        """Send a command request to a Satellite and return response."""
        self.send_request(command, payload, tags)
        msg = self.get_message()
        if msg is None:
            raise RuntimeError("Failed to get response")
        if not msg.verb_type == CSCP1Message.Type.SUCCESS:
            raise RuntimeError(msg.verb_msg)
        return msg

    def send_reply(
        self,
        response: str,
        type: CSCP1Message.Type,
        payload: Any = None,
        tags: dict[str, Any] | None = None,
    ) -> None:
        """Send a reply to a previous command with an optional payload."""
        self._dispatch((type, response), payload, tags, flags=zmq.NOBLOCK)

    def get_message(self, flags: int = 0) -> CSCP1Message | None:
        """Retrieve and return a CSCP1Message.

        Returns None if no request is waiting and `flags==zmq.NOBLOCK`.

        Raises RuntimeError if message verb is malformed.

        """
        try:
            with self._lock:
                frames = self._socket.recv_multipart(flags)
        except zmq.ZMQError as e:
            if "Resource temporarily unavailable" not in e.strerror:
                raise RuntimeError("CommandTransmitter encountered zmq exception") from e
            return None
        return CSCP1Message.disassemble(frames)

    def _dispatch(
        self, verb: tuple[CSCP1Message.Type, str], payload: Any = None, tags: dict[str, Any] | None = None, flags: int = 0
    ) -> None:
        message = CSCP1Message(self._name, verb, tags=tags)
        if payload is not None:
            message.payload = payload
        with self._lock:
            message.assemble().send(self._socket, flags)

    def close(self) -> None:
        with self._lock:
            self._socket.close()
