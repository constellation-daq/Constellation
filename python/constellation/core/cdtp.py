"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2

Module implementing the Constellation Data Transmission Protocol.
"""

from __future__ import annotations

import queue
import threading
import time
from collections.abc import Callable
from dataclasses import dataclass
from enum import IntEnum, IntFlag, auto
from typing import Any
from uuid import UUID

import zmq

from .base import ConstellationLogger
from .chirp import get_uuid
from .chirpmanager import DiscoveredService
from .message.cdtp2 import CDTP2BORMessage, CDTP2EORMessage, CDTP2Message, DataRecord


class RunCondition(IntFlag):
    """Defines the condition flags of run data as transmitted via CDTP."""

    GOOD = 0x00
    TAINTED = 0x01
    INCOMPLETE = 0x02
    INTERRUPTED = 0x04
    ABORTED = 0x08
    DEGRADED = 0x10


class TransmitterState(IntEnum):
    NOT_CONNECTED = auto()
    BOR_RECEIVED = auto()
    EOR_RECEIVED = auto()


@dataclass
class TransmitterStateSeq:
    state: TransmitterState
    seq: int
    missed: int


class SendTimeoutError(RuntimeError):
    def __init__(self, what: str, timeout: int):
        super().__init__(f"Failed sending {what} after {timeout}s")


class PushThread(threading.Thread):
    """Sending thread for CDTP"""

    def __init__(self, dtm: DataTransmitter) -> None:
        super().__init__()
        self._dtm = dtm
        self.exc: SendTimeoutError | None = None

    def _send(self, msg: CDTP2Message, current_payload_bytes: int) -> bool:
        try:
            self._dtm.log_cdtp.trace(
                "Sending data records from %s to %s (%d bytes)",
                msg.data_records[0].sequence_number,
                msg.data_records[-1].sequence_number,
                current_payload_bytes,
            )
            self._dtm._send_message(msg)
            self._dtm._bytes_transmitted += current_payload_bytes
            self._dtm._records_transmitted += len(msg.data_records)
            msg.clear_data_records()
        except zmq.error.Again:
            self.exc = SendTimeoutError("DATA message", self._dtm._data_timeout)
            self._dtm._failure_cb(str(self.exc))
            return False
        return True

    def run(self) -> None:
        """Thread method pushing data messages"""
        last_sent = time.time()
        current_payload_bytes = 0

        # Convert data payload threshold from KiB to bytes
        payload_threshold_b = self._dtm._payload_threshold * 1024

        # Preallocate message
        msg = CDTP2Message(self._dtm._name, CDTP2Message.Type.DATA)

        while not self._dtm._stopevt.is_set() and self.exc is None:
            try:
                # Get data record from queue
                data_record = self._dtm._queue.get_nowait()
                # Add to message
                current_payload_bytes += data_record.count_payload_bytes()
                msg.add_data_record(data_record)
                self._dtm._queue.task_done()
                # If threshold not reached, continue
                if current_payload_bytes < payload_threshold_b:
                    continue
            except queue.Empty:
                # Continue if send timeout is not reached yet
                if time.time() - last_sent < 0.5:
                    time.sleep(0.01)
                    continue
                # Continue if nothing to send even after timeout was reached
                if current_payload_bytes == 0:
                    last_sent = time.time()
                    continue

            # Send message
            success = self._send(msg, current_payload_bytes)
            if not success:
                return
            last_sent = time.time()
            current_payload_bytes = 0

        # Send remaining records
        if msg.data_records:
            _ = self._send(msg, current_payload_bytes)


class DataTransmitter:
    """Base class for sending CDTP messages via ZMQ."""

    def __init__(
        self,
        name: str,
        socket: zmq.Socket,  # type: ignore[type-arg]
        logger: ConstellationLogger,
        failure_cb: Callable[[str], None],
    ):
        self._name = name
        self._socket = socket
        self.log_cdtp = logger
        self._sequence_number = 0
        self._bytes_transmitted = 0
        self._records_transmitted = 0
        self._state = TransmitterState.NOT_CONNECTED

        self._queue_size = 32768
        self._payload_threshold = 128
        self._queue = queue.Queue[DataRecord](self._queue_size)
        self._stopevt = threading.Event()
        self._push_thread: PushThread | None = None
        self._failure_cb = failure_cb

        self._data_timeout: int = -1
        self._bor_timeout: int = -1
        self._eor_timeout: int = -1

    @property
    def sequence_number(self) -> int:
        return self._sequence_number

    @property
    def bytes_transmitted(self) -> int:
        return self._bytes_transmitted

    @property
    def records_transmitted(self) -> int:
        return self._records_transmitted

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

    def can_send_record(self) -> bool:
        """Check if a data record can be send immediately"""
        return not self._queue.full()

    def new_data_record(self, tags: dict[str, Any] | None = None) -> DataRecord:
        """Return new data record for sending"""
        self._sequence_number += 1
        return DataRecord(self._sequence_number, tags)

    def send_data_record(self, data_record: DataRecord) -> None:
        """Queue a data record for sending"""
        self._queue.put(data_record, block=False)

    def send_bor(self, user_tags: dict[str, Any], configuration: dict[str, Any], flags: int = 0) -> None:
        # Adjust send timeout for BOR message
        self.log_cdtp.debug("Sending BOR message with timeout %ss", self._bor_timeout)
        timeout = 1000 * self._bor_timeout if self._bor_timeout >= 0 else -1
        self._socket.setsockopt(zmq.SNDTIMEO, timeout)
        try:
            self._send_message(CDTP2BORMessage(self._name, user_tags, configuration), flags)
        except zmq.error.Again as e:
            self._state = TransmitterState.NOT_CONNECTED
            raise RuntimeError(f"Timed out sending BOR after {self._bor_timeout}s") from e
        self._state = TransmitterState.BOR_RECEIVED
        timeout = 1000 * self._data_timeout if self._data_timeout >= 0 else -1
        self._socket.setsockopt(zmq.SNDTIMEO, timeout)
        self.log_cdtp.debug("Sent BOR message")

    def send_eor(self, user_tags: dict[str, Any], run_metadata: dict[str, Any], flags: int = 0) -> None:
        # Adjust send timeout for EOR message
        self.log_cdtp.debug("Sending EOR message with timeout %ss", self._eor_timeout)
        timeout = 1000 * self._eor_timeout if self._eor_timeout >= 0 else -1
        self._socket.setsockopt(zmq.SNDTIMEO, timeout)
        try:
            self._send_message(CDTP2EORMessage(self._name, user_tags, run_metadata), flags)
        except zmq.error.Again as e:
            self._state = TransmitterState.NOT_CONNECTED
            raise RuntimeError(f"Timed out sending EOR after {self._eor_timeout}s") from e
        self._state = TransmitterState.EOR_RECEIVED
        timeout = 1000 * self._data_timeout if self._data_timeout >= 0 else -1
        self._socket.setsockopt(zmq.SNDTIMEO, timeout)
        self.log_cdtp.debug("Sent EOR message")

    def _send_message(self, msg: CDTP2Message, flags: int = 0) -> None:
        msg.assemble().send(self._socket, flags)

    def start_sending(self) -> None:
        # Reset stop event, sequence number, bytes transmitted and re-create queue
        self._stopevt.clear()
        self._sequence_number = 0
        self._bytes_transmitted = 0
        self._records_transmitted = 0
        self._queue = queue.Queue[DataRecord](self._queue_size)
        # Set send timeout for DATA messages
        timeout = 1000 * self._data_timeout if self._data_timeout >= 0 else -1
        self._socket.setsockopt(zmq.SNDTIMEO, timeout)
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
        """Thread method pulling data messages"""
        try:
            while not self._drc._stopevt.is_set():
                with self._drc._poller_lock:
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
        receive_data_cb: Callable[[str, DataRecord], None],
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
        self._poller_lock = threading.Lock()
        self._sockets: dict[UUID, zmq.Socket] = {}  # type: ignore[type-arg]

        self._data_transmitters = data_transmitters
        self._data_transmitter_states = dict[str, TransmitterStateSeq]()
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
    def data_transmitters(self) -> set[str] | None:
        return self._data_transmitters

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
            data_transmitters_not_connected = [
                key for key, value in self._data_transmitter_states.items() if value.state == TransmitterState.NOT_CONNECTED
            ]
            if len(data_transmitters_not_connected) > 0:
                self.log_cdtp.warning("BOR message never send from %s", data_transmitters_not_connected)
            # Loop until all data transmitters that sent a BOR also sent an EOR
            while True:
                # Filter for data transmitters that did not send an EOR
                data_transmitters_no_eor = {
                    key: value
                    for key, value in self._data_transmitter_states.items()
                    if value.state == TransmitterState.BOR_RECEIVED
                }
                # Check if EOR messages are missing
                if len(data_transmitters_no_eor) == 0:
                    break
                # If timeout reached, throw
                if time.time() - time_stopped > self._eor_timeout:
                    # Stop thread
                    self._stop_pull_thread()
                    # Create substitute EORs
                    self.log_cdtp.warning(
                        "Not all EOR messages received, emitting substitute EOR messages for %s",
                        list(data_transmitters_no_eor.keys()),
                    )
                    for data_transmitter, state_seq in data_transmitters_no_eor.items():
                        self.log_cdtp.debug("Creating substitute EOR for %s", data_transmitter)
                        condition_code = RunCondition.ABORTED
                        if state_seq.missed > 0:
                            condition_code |= RunCondition.INCOMPLETE
                        # TODO: check degraded
                        run_metadata = {
                            "condition_code": condition_code.value,
                            "condition": condition_code.name,
                        }
                        self._receive_eor(data_transmitter, {}, run_metadata)
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
        with self._poller_lock:
            socket = self._context.socket(zmq.PULL)
            socket.connect(f"tcp://{address}:{port}")
            self._sockets[uuid] = socket
            self._poller.register(socket, zmq.POLLIN)

    def _remove_socket(self, uuid: UUID) -> None:
        with self._poller_lock:
            socket = self._sockets.pop(uuid)
            self._poller.unregister(socket)
            socket.close()

    def _reset_sockets(self) -> None:
        with self._poller_lock:
            for socket in self._sockets.values():
                self._poller.unregister(socket)
                socket.close()
            self._sockets.clear()

    def _reset_data_transmitter_states(self) -> None:
        self._data_transmitter_states.clear()
        if self._data_transmitters is not None:
            for data_transmitter in self._data_transmitters:
                self._data_transmitter_states[data_transmitter] = TransmitterStateSeq(TransmitterState.NOT_CONNECTED, 0, 0)

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
            and self._data_transmitter_states[msg.sender].state != TransmitterState.NOT_CONNECTED
        ):
            raise InvalidCDTPMessageType(msg.type, f"already received BOR from {msg.sender}")

        # Udate state
        self._data_transmitter_states[msg.sender] = TransmitterStateSeq(TransmitterState.BOR_RECEIVED, 0, 0)

        # BOR callback
        self._receive_bor(msg.sender, msg.user_tags, msg.configuration)

    def _handle_data_message(self, msg: CDTP2Message) -> None:
        self.log_cdtp.trace(
            "Received data message from %s with data records from %s to %s",
            msg.sender,
            msg.data_records[0].sequence_number,
            msg.data_records[-1].sequence_number,
        )

        # Check that BOR was received
        self._check_bor_received(CDTP2Message.Type.DATA, msg.sender)

        # Store iterator of dict to avoid multiple lookups
        data_transmitter_it = self._data_transmitter_states[msg.sender]

        for data_record in msg.data_records:
            # Store sequence number and missed messages
            data_transmitter_it.missed += data_record.sequence_number - 1 - data_transmitter_it.seq
            data_transmitter_it.seq = data_record.sequence_number

            # Data record callback
            self._receive_data(msg.sender, data_record)

    def _handle_eor_message(self, msg: CDTP2EORMessage) -> None:
        self.log_cdtp.info("Received EOR from %s with run metadata %s", msg.sender, msg.run_metadata)

        # Check that BOR was received
        self._check_bor_received(CDTP2Message.Type.EOR, msg.sender)

        # Store iterator of dict to avoid multiple lookups
        data_transmitter_it = self._data_transmitter_states[msg.sender]

        # Update state
        data_transmitter_it.state = TransmitterState.EOR_RECEIVED

        # Check for missed message and update metadata
        if data_transmitter_it.missed > 0:
            self._apply_run_condition(msg, RunCondition.INCOMPLETE)
        # TODO: check degraded

        # EOR callback
        self._receive_eor(msg.sender, msg.user_tags, msg.run_metadata)

    def _apply_run_condition(self, msg: CDTP2EORMessage, condition_code: RunCondition) -> None:
        try:
            condition_code = RunCondition(msg.run_metadata["condition_code"]) | condition_code
        except KeyError:
            pass
        msg.run_metadata["condition_code"] = condition_code.value
        msg.run_metadata["condition"] = condition_code.name

    def _check_bor_received(self, type: CDTP2Message.Type, sender: str) -> None:
        # If not in states or state not BOR_RECEIVED, throw
        try:
            if self._data_transmitter_states[sender].state == TransmitterState.BOR_RECEIVED:
                return
        except KeyError:
            pass
        raise InvalidCDTPMessageType(type, f"did not receive BOR from {sender}")
