"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2

Module implementing the Constellation Data Transmission Protocol.
"""

from __future__ import annotations

import queue
import threading
import time
from collections import Counter
from collections.abc import Callable
from enum import IntEnum, auto
from typing import Any
from uuid import UUID

import zmq

from .base import ConstellationLogger
from .chirp import get_uuid
from .chirpmanager import DiscoveredService
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


class RecvTimeoutError(RuntimeError):
    def __init__(self, what: str, timeout: int):
        super().__init__(f"Failed receiving {what} after {timeout}s")


class InvalidCDTPMessageType(RuntimeError):
    def __init__(self, type: CDTP2Message.Type, reason: str):
        super().__init__(f"Error handling CDTP message with type {type.name}: {reason}")


class PullThread(threading.Thread):
    """Receiving thread for CDTP"""

    def __init__(self, drc: DataReceiver) -> None:
        super().__init__()
        self._drc = drc
        self.exc: BaseException | None = None

    def run(self) -> None:
        try:
            while not self._drc._stopevt.is_set():
                sockets_ready = dict(self._drc._poller.poll(timeout=50))
                self._drc._poller_events = len(sockets_ready)
                for socket in sockets_ready.keys():
                    frames = socket.recv_multipart()
                    msg = CDTP2Message.disassemble(frames)
                    self._drc._handle_cdtp_message(msg)
        except BaseException as e:
            self.exc = e


class DataReceiver:
    """Base class for receiving CDTP messages via ZMQ."""

    def __init__(
        self,
        context: zmq.Context,  # type: ignore[type-arg]
        logger: ConstellationLogger,
        receive_bor_cb: Callable[[str, dict[str, Any], dict[str, Any]], None],
        receive_data_cb: Callable[[str, DataBlock], None],
        receive_eor_cb: Callable[[str, dict[str, Any], dict[str, Any]], None],
        data_transmitters: set[str] | None,
    ):
        self._context = context
        self.log_cdtp = logger
        self._receive_bor = receive_bor_cb
        self._receive_data = receive_data_cb
        self._receive_eor = receive_eor_cb
        self._poller = zmq.Poller()
        self._poller_events = 0
        self._sockets: dict[UUID, zmq.Socket] = {}  # type: ignore[type-arg]

        self._data_transmitters = data_transmitters
        self._data_transmitter_states = dict[str, TransmitterState]()
        self._bytes_received = 0

        self._eor_timeout = 10

        self._stopevt = threading.Event()
        self._pull_thread: PullThread | None = None
        self._running = False

    @property
    def bytes_received(self) -> int:
        return self._bytes_received

    @property
    def running(self) -> bool:
        return self._running

    @property
    def eor_timeout(self) -> int:
        """The EOR receiving timeout value (in s)."""
        return self._eor_timeout

    @eor_timeout.setter
    def eor_timeout(self, timeout: int) -> None:
        self._eor_timeout = timeout

    def start_receiving(self) -> None:
        # Reset stop event, data transmitter states and bytes received
        self._stopevt.clear()
        self._reset_data_transmitter_states()
        self._bytes_received = 0
        # Start receiving thread
        self.log_cdtp.debug("Starting pull thread")
        self._pull_thread = PullThread(self)
        self._pull_thread.start()
        self._running = True

    def stop_receiving(self) -> None:
        if self._pull_thread is not None:
            # Wait until no more events returned by poller
            while self._poller_events > 0 and self._pull_thread.is_alive():
                self.log_cdtp.trace("Poller still returned events, waiting before checking for EOR arrivals")
                time.sleep(0.1)
            # Now start EOR timer
            time_stopped = time.time()
            self.log_cdtp.debug("Starting timeout for EOR arrivals (%ss)", self._eor_timeout)
            # Warn about transmitters that never sent a BOR message
            data_transmitters_not_connected = {
                key: value for key, value in self._data_transmitter_states.items() if value == TransmitterState.NOT_CONNECTED
            }
            if len(data_transmitters_not_connected) > 0:
                self.log_cdtp.warning("BOR message never send from %s", data_transmitters_not_connected.keys())
            # Loop until all data transmitters that sent a BOR also sent an EOR
            while True:
                # Check if EOR messages are missing
                missing_eors = Counter(self._data_transmitter_states.values())[TransmitterState.BOR_RECEIVED]
                if missing_eors == 0:
                    break
                # If timeout reached, throw
                if time.time() - time_stopped > self._eor_timeout:
                    # Stop thread
                    self._stop_pull_thread()
                    # Filter for data transmitters that did not send an EOR
                    data_transmitters_no_eor = [
                        key for key, value in self._data_transmitter_states.items() if value == TransmitterState.BOR_RECEIVED
                    ]
                    self.log_cdtp.warning(
                        "Not all EOR messages received, emitting substitute EOR messages for %s",
                        list(data_transmitters_no_eor.keys()),
                    )
                    # Create substitute EORs
                    pass  # TODO
                    # Raise ReceiveTimeoutError so that we can catch this scenario in interrupting
                    raise RecvTimeoutError(f"EOR messages from {list(data_transmitters_no_eor.keys())}", self._eor_timeout)
                # Sleep a bit to avoid hot-loop
                time.sleep(0.05)

            self.log_cdtp.debug("All EOR messages received")

            # Stop and join thread
            self._stop_pull_thread()

    def check_exception(self) -> None:
        if self._pull_thread is not None:
            if self._pull_thread.exc is not None:
                # Reset sockets
                self._reset_sockets()
                # Then raise
                raise self._pull_thread.exc

    def _stop_pull_thread(self) -> None:
        if self._pull_thread is not None:
            self.log_cdtp.debug("Joining pull thread")
            self._stopevt.set()
            self._pull_thread.join()
            self._reset_sockets()
            self._running = False

    def add_sender(self, service: DiscoveredService) -> None:
        # Check that pull thread is running
        if self._pull_thread is None or not self._pull_thread.is_alive():
            return
        # If data transmitters exists skip if not contained in set
        if self._data_transmitters is not None:
            if service.host_uuid not in [get_uuid(name) for name in self._data_transmitters]:
                return
        self._add_socket(service.host_uuid, service.address, service.port)

    def remove_sender(self, service: DiscoveredService) -> None:
        # Check that pull thread is running
        if self._pull_thread is None or not self._pull_thread.is_alive():
            return
        # Remove if in set
        if service.host_uuid in self._sockets:
            self._remove_socket(service.host_uuid)

    def _add_socket(self, uuid: UUID, address: str, port: int) -> None:
        socket = self._context.socket(zmq.PULL)
        socket.connect(f"tcp://{address}:{port}")
        self._sockets[uuid] = socket
        self._poller.register(socket, zmq.POLLIN)

    def _remove_socket(self, uuid: UUID) -> None:
        socket = self._sockets.pop(uuid)
        self._poller.unregister(socket)
        socket.close()

    def _reset_sockets(self) -> None:
        for socket in self._sockets.values():
            self._poller.unregister(socket)
            socket.close()
        self._sockets.clear()

    def _reset_data_transmitter_states(self) -> None:
        self._data_transmitter_states.clear()
        if self._data_transmitters is not None:
            for data_transmitter in self._data_transmitters:
                self._data_transmitter_states[data_transmitter] = TransmitterState.NOT_CONNECTED

    def _handle_cdtp_message(self, msg: CDTP2Message) -> None:
        if msg.type == CDTP2Message.Type.DATA:
            self._bytes_received += msg.count_payload_bytes()
            self._handle_data_message(msg)
        elif msg.type == CDTP2Message.Type.BOR:
            self._handle_bor_message(CDTP2BORMessage.cast(msg))
        else:
            self._handle_eor_message(CDTP2EORMessage.cast(msg))

    def _handle_bor_message(self, msg: CDTP2BORMessage) -> None:
        self.log_cdtp.info("Received BOR from %s with config %s", msg.sender, msg.configuration)

        # If registered in states, raise if already connected
        if (
            msg.sender in self._data_transmitter_states
            and self._data_transmitter_states[msg.sender] != TransmitterState.NOT_CONNECTED
        ):
            raise InvalidCDTPMessageType(msg.type, f"already received BOR from {msg.sender}")

        # Udate state
        self._data_transmitter_states[msg.sender] = TransmitterState.BOR_RECEIVED

        # BOR callback
        self._receive_bor(msg.sender, msg.user_tags, msg.configuration)

    def _handle_data_message(self, msg: CDTP2Message) -> None:
        self.log_cdtp.trace(
            "Received data message from %s with data blocks from %s to %s",
            msg.sender,
            msg.data_blocks[0].sequence_number,
            msg.data_blocks[-1].sequence_number,
        )

        # Check that BOR was received
        self._check_bor_received(CDTP2Message.Type.DATA, msg.sender)

        # Data callback for each data block
        for data_block in msg.data_blocks:
            self._receive_data(msg.sender, data_block)

    def _handle_eor_message(self, msg: CDTP2EORMessage) -> None:
        self.log_cdtp.info("Received EOR from %s with run metadata %s", msg.sender, msg.run_metadata)

        # Check that BOR was received
        self._check_bor_received(CDTP2Message.Type.EOR, msg.sender)

        # Update state
        self._data_transmitter_states[msg.sender] = TransmitterState.EOR_RECEIVED

        # EOR callback
        self._receive_eor(msg.sender, msg.user_tags, msg.run_metadata)

    def _check_bor_received(self, type: CDTP2Message.Type, sender: str) -> None:
        # If not in states or state not BOR_RECEIVED, throw
        try:
            if self._data_transmitter_states[sender] == TransmitterState.BOR_RECEIVED:
                return
        except KeyError:
            pass
        raise InvalidCDTPMessageType(type, f"did not receive BOR from {sender}")
