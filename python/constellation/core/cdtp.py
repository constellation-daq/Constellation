"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2

Module implementing the Constellation Data Transmission Protocol.
"""

from __future__ import annotations

import queue
import threading
import time
from enum import IntEnum, auto
from typing import Any

import zmq

from .base import ConstellationLogger
from .message.cdtp2 import CDTP2BORMessage, CDTP2EORMessage, CDTP2Message, DataBlock


class TransmitterState(IntEnum):
    NOT_CONNECTED = auto()
    BOR_RECEIVED = auto()
    EOR_RECEIVED = auto()


class SendTimeoutError(RuntimeError):
    def __init__(self, what: str, timeout: int):
        super().__init__(f"Failed sending {what} after {timeout}s")


class PushThread(threading.Thread):
    """Sending thread for CDTP"""

    def __init__(self, dtm: DataTransmitter) -> None:
        super().__init__()
        self._dtm = dtm
        self.exc: SendTimeoutError | None = None

    def _send(self, msg: CDTP2Message) -> None:
        try:
            self._dtm.log_cdtp.trace(
                "Sending data blocks from %s to %s",
                msg.data_blocks[0].sequence_number,
                msg.data_blocks[-1].sequence_number,
            )
            self._dtm._send_message(msg)
            msg.clear_data_blocks()
        except zmq.error.Again:
            self.exc = SendTimeoutError("DATA message", self._dtm._data_timeout)
            self._dtm.log_cdtp.critical("%s", self.exc)

    def run(self) -> None:
        last_sent = time.time()
        current_payload_bytes = 0

        # Convert data payload threshold from KiB to bytes
        payload_threshold_b = self._dtm._payload_threshold * 1024

        # Preallocate message
        msg = CDTP2Message(self._dtm._name, CDTP2Message.Type.DATA)

        while not self._dtm._stopevt.is_set() and self.exc is None:
            try:
                # Get data block from queue
                data_block = self._dtm._queue.get_nowait()
                # Add to message
                current_payload_bytes += data_block.count_payload_bytes()
                msg.add_data_block(data_block)
                self._dtm._queue.task_done()
                # If threshold not reached, continue
                if current_payload_bytes < payload_threshold_b:
                    continue
                # Send message
                self._send(msg)
                last_sent = time.time()
                current_payload_bytes = 0
            except queue.Empty:
                # Continue if send timeout is not reached yet
                if time.time() - last_sent < 0.5:
                    time.sleep(0.01)
                    continue
                # Continue if nothing to send even after timeout was reached
                if current_payload_bytes == 0:
                    last_sent = time.time()
                    continue
                # Otherwise send message
                self._send(msg)
                last_sent = time.time()
                current_payload_bytes = 0


class DataTransmitter:
    """Base class for sending CDTP messages via ZMQ."""

    def __init__(self, name: str, socket: zmq.Socket, logger: ConstellationLogger):  # type: ignore[type-arg]
        self._name = name
        self._socket = socket
        self.log_cdtp = logger
        self._sequence_number = 0
        self._state = TransmitterState.NOT_CONNECTED

        self._queue_size = 32768
        self._payload_threshold = 128
        self._queue = queue.Queue[DataBlock](self._queue_size)
        self._stopevt = threading.Event()
        self._push_thread: PushThread | None = None

        self._data_timeout: int = -1
        self._bor_timeout: int = -1
        self._eor_timeout: int = -1

    @property
    def sequence_number(self) -> int:
        return self._sequence_number

    @property
    def state(self) -> TransmitterState:
        return self._state

    @property
    def eor_timeout(self) -> int:
        """The EOR sending timeout value (in s)."""
        return self._eor_timeout

    @eor_timeout.setter
    def eor_timeout(self, timeout: int) -> None:
        self._eor_timeout = timeout

    @property
    def bor_timeout(self) -> int:
        """The BOR sending timeout value (in s)."""
        return self._bor_timeout

    @bor_timeout.setter
    def bor_timeout(self, timeout: int) -> None:
        self._bor_timeout = timeout

    @property
    def data_timeout(self) -> int:
        """The DATA sending timeout value (in s)."""
        return self._data_timeout

    @data_timeout.setter
    def data_timeout(self, timeout: int) -> None:
        self._data_timeout = timeout

    @property
    def payload_threshold(self) -> int:
        return self._payload_threshold

    @payload_threshold.setter
    def payload_threshold(self, new_payload_threshold: int) -> None:
        self._payload_threshold = new_payload_threshold

    @property
    def queue_size(self) -> int:
        return self._queue_size

    @queue_size.setter
    def queue_size(self, new_queue_size: int) -> None:
        self._queue_size = new_queue_size

    def check_rate_limited(self) -> bool:
        """Check if the satellite is currently rate limited"""
        return self._queue.full()

    def new_data_block(self, tags: dict[str, Any] | None = None) -> DataBlock:
        """Return new data block for sending"""
        self._sequence_number += 1
        return DataBlock(self._sequence_number, tags)

    def send_data_block(self, data_block: DataBlock) -> None:
        """Queue a data block for sending"""
        self._queue.put(data_block, block=False)

    def send_bor(self, user_tags: dict[str, Any], configuration: dict[str, Any], flags: int = 0) -> None:
        # Adjust send timeout for BOR message
        self.log_cdtp.debug("Sending BOR message with timeout %ss", self._bor_timeout)
        self._socket.setsockopt(zmq.SNDTIMEO, 1000 * self._bor_timeout)
        try:
            self._send_message(CDTP2BORMessage(self._name, user_tags, configuration), flags)
        except zmq.error.Again as e:
            self._state = TransmitterState.NOT_CONNECTED
            raise RuntimeError(f"Timed out sending BOR after {self._bor_timeout}s") from e
        self._state = TransmitterState.BOR_RECEIVED
        self._socket.setsockopt(zmq.SNDTIMEO, 1000 * self._data_timeout)
        self.log_cdtp.debug("Sent BOR message")

    def send_eor(self, user_tags: dict[str, Any], run_metadata: dict[str, Any], flags: int = 0) -> None:
        # Adjust send timeout for EOR message
        self.log_cdtp.debug("Sending EOR message with timeout %ss", self._eor_timeout)
        self._socket.setsockopt(zmq.SNDTIMEO, 1000 * self._eor_timeout)
        try:
            self._send_message(CDTP2EORMessage(self._name, user_tags, run_metadata), flags)
        except zmq.error.Again as e:
            self._state = TransmitterState.NOT_CONNECTED
            raise RuntimeError(f"Timed out sending EOR after {self._eor_timeout}s") from e
        self._state = TransmitterState.EOR_RECEIVED
        self._socket.setsockopt(zmq.SNDTIMEO, 1000 * self._data_timeout)
        self.log_cdtp.debug("Sent EOR message")

    def _send_message(self, msg: CDTP2Message, flags: int = 0) -> None:
        msg.assemble().send(self._socket, flags)

    def start_sending(self) -> None:
        # Reset sequence number, stop event and re-create queue
        self._sequence_number = 0
        self._stopevt.clear()
        self._queue = queue.Queue[DataBlock](self._queue_size)
        # Set send timeout for DATA messages
        self._socket.setsockopt(zmq.SNDTIMEO, 1000 * self._data_timeout)
        # Start sending thread
        self.log_cdtp.debug("Starting push thread")
        self._push_thread = PushThread(self)
        self._push_thread.start()

    def stop_sending(self) -> None:
        if self._push_thread is not None:
            # Wait until queue is empty
            self.log_cdtp.debug("Waiting until data queue is empty")
            while not self._queue.empty() and self._push_thread.is_alive():
                time.sleep(0.1)
            # Stop and join thread
            self.log_cdtp.debug("Joining push thread")
            self._stopevt.set()
            self._push_thread.join()

    def check_exception(self) -> None:
        if self._push_thread is not None:
            if self._push_thread.exc is not None:
                raise self._push_thread.exc
