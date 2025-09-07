"""
SPDX-FileCopyrightText: 2025 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2
"""

import threading
import time
from typing import Any

import pytest
import zmq
from conftest import DATA_PORT, wait_for_state

from constellation.core.cdtp import (
    DataReceiver,
    DataTransmitter,
    InvalidCDTPMessageType,
    RecvTimeoutError,
    RunCondition,
    TransmitterState,
)
from constellation.core.chirp import CHIRPServiceIdentifier, get_uuid
from constellation.core.chirpmanager import DiscoveredService
from constellation.core.cscp import CommandTransmitter
from constellation.core.logging import ConstellationLogger
from constellation.core.message.cdtp2 import DataRecord
from constellation.core.message.cscp1 import SatelliteState
from constellation.core.network import get_loopback_interface_name
from constellation.core.receiver_satellite import ReceiverSatellite
from constellation.core.transmitter_satellite import TransmitterSatellite


@pytest.fixture
def mock_data_transmitter(mock_socket_sender):
    mock_socket_sender.port = DATA_PORT
    t = DataTransmitter("mock_transmitter", mock_socket_sender, ConstellationLogger("mock_transmitter"), lambda x: None)
    yield t


class MockDataReceiver(DataReceiver):
    def __init__(
        self,
        context: zmq.Context,  # type: ignore[type-arg]
        data_transmitters: set[str] | None,
    ):
        super().__init__(
            context, ConstellationLogger("mock_receiver"), self.store_bor, self.store_data, self.store_eor, data_transmitters
        )
        self.last_bors: list[tuple[str, dict[str, Any], dict[str, Any]]] = []
        self.last_data_records: list[tuple[str, DataRecord]] = []
        self.last_eors: list[tuple[str, dict[str, Any], dict[str, Any]]] = []

    def store_bor(self, sender: str, user_tags: dict[str, Any], configuration: dict[str, Any]):
        self.last_bors.append((sender, user_tags, configuration))

    def store_data(self, sender: str, data_record: DataRecord):
        self.last_data_records.append((sender, data_record))

    def store_eor(self, sender: str, user_tags: dict[str, Any], run_metadata: dict[str, Any]):
        self.last_eors.append((sender, user_tags, run_metadata))


@pytest.fixture
def mock_data_receiver(mock_poller):
    ctx, poller = mock_poller
    r = MockDataReceiver(
        ctx,
        {"mock_transmitter", "mock_transmitter_2"},
    )
    yield r


class DummyTransmitterSatellite(TransmitterSatellite):
    def __init__(self, *args: Any, **kwargs: Any):
        super().__init__(*args, **kwargs)
        self.throw_run = False

    def do_starting(self, run_identifier: str) -> str:
        self.bor = {"dummy": True}
        return "Started"

    def do_stopping(self) -> str:
        self.eor = {"data_collected": False}
        return "Stopped"

    def do_run(self, run_identifier: str) -> str:
        if self.throw_run:
            time.sleep(0.05)
            raise Exception("throwing in RUN as requested")
        return "Finished run"


@pytest.fixture
def transmitter_satellite():
    s = DummyTransmitterSatellite(
        name="mock_receiver",
        group="mockstellation",
        cmd_port=11111,
        hb_port=22222,
        mon_port=33333,
        data_port=DATA_PORT,
        interface=[get_loopback_interface_name()],
    )
    t = threading.Thread(target=s.run_satellite)
    t.start()
    # give the threads a chance to start
    time.sleep(0.2)
    yield s
    # teardown
    s.terminate()


class DummyReceiverSatellite(ReceiverSatellite):
    def __init__(self, *args: Any, **kwargs: Any):
        super().__init__(*args, **kwargs)
        self.throw_run = False
        self.last_bors: list[tuple[str, dict[str, Any], dict[str, Any]]] = []
        self.last_data_records: list[tuple[str, DataRecord]] = []
        self.last_eors: list[tuple[str, dict[str, Any], dict[str, Any]]] = []

    def receive_bor(self, sender: str, user_tags: dict[str, Any], configuration: dict[str, Any]):
        self.last_bors.append((sender, user_tags, configuration))

    def receive_data(self, sender: str, data_record: DataRecord):
        self.last_data_records.append((sender, data_record))

    def receive_eor(self, sender: str, user_tags: dict[str, Any], run_metadata: dict[str, Any]):
        self.last_eors.append((sender, user_tags, run_metadata))

    def do_run(self, run_identifier: str) -> str:
        if self.throw_run:
            time.sleep(0.05)
            raise Exception("throwing in RUN as requested")
        return super().do_run(run_identifier)


@pytest.fixture
def receiver_satellite():
    s = DummyReceiverSatellite(
        name="mock_receiver",
        group="mockstellation",
        cmd_port=11112,
        hb_port=22223,
        mon_port=33334,
        interface=[get_loopback_interface_name()],
    )
    t = threading.Thread(target=s.run_satellite)
    t.start()
    # give the threads a chance to start
    time.sleep(0.2)
    yield s
    # teardown
    s.terminate()


@pytest.fixture
def cmd_transmitter():
    ctx = zmq.Context()
    socket_1 = ctx.socket(zmq.REQ)
    socket_1.connect("tcp://127.0.0.1:11111")
    cmd_tx_1 = CommandTransmitter("cmd_tx_1", socket_1)
    socket_2 = ctx.socket(zmq.REQ)
    socket_2.connect("tcp://127.0.0.1:11112")
    cmd_tx_2 = CommandTransmitter("cmd_tx_2", socket_2)
    yield cmd_tx_1, cmd_tx_2
    # teardown
    cmd_tx_1.close()
    cmd_tx_2.close()
    ctx.term()


def test_datatransmitter_cov(mock_data_transmitter: DataTransmitter):
    transmitter = mock_data_transmitter
    transmitter.check_exception()
    assert transmitter.state == TransmitterState.NOT_CONNECTED
    assert transmitter.can_send_record()
    transmitter.new_data_record()
    assert transmitter.sequence_number == 1
    transmitter.bor_timeout = 2
    assert transmitter.bor_timeout == 2
    transmitter.eor_timeout = 3
    assert transmitter.eor_timeout == 3
    transmitter.data_timeout = 1
    assert transmitter.data_timeout == 1
    transmitter.payload_threshold = 64
    assert transmitter.payload_threshold == 64
    transmitter.queue_size = 65536
    assert transmitter.queue_size == 65536


def test_datareceiver_cov(mock_data_receiver: DataReceiver):
    receiver = mock_data_receiver
    receiver.check_exception()
    assert receiver.bytes_received == 0
    assert not receiver.running
    assert receiver.data_transmitters is not None
    receiver.eor_timeout = 3
    assert receiver.eor_timeout == 3
    tx_service = DiscoveredService(get_uuid("mock_transmitter"), CHIRPServiceIdentifier.DATA, "127.0.0.1", DATA_PORT)
    receiver.add_sender(tx_service)
    receiver.remove_sender(tx_service)


def test_data_transmission(mock_data_transmitter: DataTransmitter, mock_data_receiver: MockDataReceiver):
    transmitter = mock_data_transmitter
    receiver = mock_data_receiver

    # Start receiver, add service for sender
    receiver.start_receiving()
    tx_service = DiscoveredService(get_uuid("mock_transmitter"), CHIRPServiceIdentifier.DATA, "127.0.0.1", DATA_PORT)
    receiver.add_sender(tx_service)

    # Send BOR and start transmitter
    bor_tags = {"tag1": 1}
    bor_config = {"tag2": 2}
    transmitter.send_bor(bor_tags, bor_config)
    transmitter.start_sending()

    # Wait for BOR
    timeout = 1.0
    while len(receiver.last_bors) < 1 and timeout > 0:
        time.sleep(0.05)
        timeout -= 0.05
    assert len(receiver.last_bors) == 1, "BOR not received"
    bor_rx_sender, bor_rx_tags, bor_rx_config = receiver.last_bors[-1]
    assert bor_rx_sender == "mock_transmitter"
    assert bor_rx_tags == bor_tags
    assert bor_rx_config == bor_config

    # Send data record
    data = b"1234"
    data_tags = {"dummy": True}
    data_record = transmitter.new_data_record(data_tags)
    data_record.add_block(data)
    transmitter.send_data_record(data_record)

    # Wait for data record
    timeout = 1.0
    while len(receiver.last_data_records) < 1 and timeout > 0:
        time.sleep(0.05)
        timeout -= 0.05
    assert len(receiver.last_data_records) == 1, "Data not received"
    data_rx_sender, data_rx_data_record = receiver.last_data_records[-1]
    assert data_rx_sender == "mock_transmitter"
    assert data_rx_data_record.sequence_number == 1
    assert data_rx_data_record.tags == data_tags
    assert len(data_rx_data_record.blocks) == 1
    assert data_rx_data_record.blocks[0] == data
    assert receiver.bytes_received == len(data)

    # Stop transmitter and send EOR
    transmitter.check_exception()
    transmitter.stop_sending()
    eor_tags = {"tag3": 3}
    eor_run_meta = {"tag4": 4}
    transmitter.send_eor(eor_tags, eor_run_meta)

    # Wait for EOR
    timeout = 1.0
    while len(receiver.last_eors) < 1 and timeout > 0:
        time.sleep(0.05)
        timeout -= 0.05
    assert len(receiver.last_eors) == 1, "EOR not received"
    eor_rx_sender, eor_rx_tags, eor_rx_run_meta = receiver.last_eors[-1]
    assert eor_rx_sender == "mock_transmitter"
    assert eor_rx_tags == eor_tags
    assert eor_rx_run_meta == eor_run_meta

    # Stop receiver
    receiver.check_exception()
    receiver.stop_receiving()


def test_datareceiver_no_eor(mock_data_transmitter: DataTransmitter, mock_data_receiver: MockDataReceiver):
    transmitter = mock_data_transmitter
    receiver = mock_data_receiver

    # Set EOR timeout
    receiver.eor_timeout = 0

    # Start receiver, add service for sender
    receiver.start_receiving()
    tx_service = DiscoveredService(get_uuid("mock_transmitter"), CHIRPServiceIdentifier.DATA, "127.0.0.1", DATA_PORT)
    receiver.add_sender(tx_service)

    # Send BOR
    transmitter.send_bor({}, {})

    # Wait for BOR
    timeout = 1.0
    while len(receiver.last_bors) < 1 and timeout > 0:
        time.sleep(0.05)
        timeout -= 0.05
    assert len(receiver.last_bors) == 1, "BOR not received"

    # Remove transmitter
    receiver.remove_sender(tx_service)

    # Stop receiver -> throws due to missing EOR
    with pytest.raises(RecvTimeoutError) as excinfo:
        receiver.stop_receiving()
    assert "Failed receiving EOR messages from ['mock_transmitter'] after 0s" in str(excinfo)


def test_datareceiver_double_bor(mock_data_transmitter: DataTransmitter, mock_data_receiver: MockDataReceiver):
    transmitter = mock_data_transmitter
    receiver = mock_data_receiver

    # Start receiver, add service for sender
    receiver.start_receiving()
    tx_service = DiscoveredService(get_uuid("mock_transmitter"), CHIRPServiceIdentifier.DATA, "127.0.0.1", DATA_PORT)
    receiver.add_sender(tx_service)

    # Send BOR
    transmitter.send_bor({}, {})

    # Wait for BOR
    timeout = 1.0
    while len(receiver.last_bors) < 1 and timeout > 0:
        time.sleep(0.05)
        timeout -= 0.05
    assert len(receiver.last_bors) == 1, "BOR not received"

    # Send another BOR
    transmitter.send_bor({}, {})

    # Wait until exception is set
    timeout = 1.0
    assert receiver._pull_thread is not None
    while receiver._pull_thread.exc is None and timeout > 0:
        time.sleep(0.05)
        timeout -= 0.05
    assert receiver._pull_thread.exc is not None

    # Check exception
    with pytest.raises(InvalidCDTPMessageType) as excinfo:
        receiver.check_exception()
    assert "Error handling CDTP message with type BOR: already received BOR from mock_transmitter" in str(excinfo)


def test_datareceiver_no_bor(mock_data_transmitter: DataTransmitter, mock_data_receiver: MockDataReceiver):
    transmitter = mock_data_transmitter
    receiver = mock_data_receiver

    # Start receiver, add service for sender
    receiver.start_receiving()
    tx_service = DiscoveredService(get_uuid("mock_transmitter"), CHIRPServiceIdentifier.DATA, "127.0.0.1", DATA_PORT)
    receiver.add_sender(tx_service)

    # Send EOR
    transmitter.send_eor({}, {})

    # Wait until exception is set
    timeout = 1.0
    assert receiver._pull_thread is not None
    while receiver._pull_thread.exc is None and timeout > 0:
        time.sleep(0.05)
        timeout -= 0.05
    assert receiver._pull_thread.exc is not None

    # Check exception
    with pytest.raises(InvalidCDTPMessageType) as excinfo:
        receiver.check_exception()
    assert "Error handling CDTP message with type EOR: did not receive BOR from mock_transmitter" in str(excinfo)


def test_datareceiver_missed_data(mock_data_transmitter: DataTransmitter, mock_data_receiver: MockDataReceiver):
    transmitter = mock_data_transmitter
    receiver = mock_data_receiver

    # Adjust data timeout and payload threshold
    transmitter.data_timeout = 0
    transmitter.payload_threshold = 0

    # Start receiver, add service for sender
    receiver.start_receiving()
    tx_service = DiscoveredService(get_uuid("mock_transmitter"), CHIRPServiceIdentifier.DATA, "127.0.0.1", DATA_PORT)
    receiver.add_sender(tx_service)

    # Send BOR and start transmitter
    transmitter.send_bor({}, {})
    transmitter.start_sending()

    # Wait for BOR
    timeout = 1.0
    while len(receiver.last_bors) < 1 and timeout > 0:
        time.sleep(0.05)
        timeout -= 0.05
    assert len(receiver.last_bors) == 1, "BOR not received"

    # Send first data record
    data_record = transmitter.new_data_record()
    data_record.add_block(b"123")
    transmitter.send_data_record(data_record)

    # Send second data record while skipping a sequence number
    data_record = transmitter.new_data_record()
    data_record._sequence_number += 1
    data_record.add_block(b"789")
    transmitter.send_data_record(data_record)

    # Wait for data records
    timeout = 1.0
    while len(receiver.last_data_records) < 2 and timeout > 0:
        time.sleep(0.05)
        timeout -= 0.05
    assert len(receiver.last_data_records) == 2, "Data not received"

    # Stop transmitter and send EOR
    transmitter.stop_sending()
    transmitter.send_eor({}, {})

    # Wait for EOR
    timeout = 1.0
    while len(receiver.last_eors) < 1 and timeout > 0:
        time.sleep(0.05)
        timeout -= 0.05
    assert len(receiver.last_eors) == 1, "EOR not received"

    # Check condition code
    eor_exp_run_condition = RunCondition.INCOMPLETE
    eor_rx_sender, eor_rx_tags, eor_rx_run_meta = receiver.last_eors[-1]
    assert "condition_code" in eor_rx_run_meta.keys()
    assert eor_rx_run_meta["condition_code"] == eor_exp_run_condition.value
    assert eor_rx_run_meta["condition"] == eor_exp_run_condition.name

    # Stop receiver
    receiver.stop_receiving()


def test_data_satellites(
    transmitter_satellite: DummyTransmitterSatellite, receiver_satellite: DummyReceiverSatellite, cmd_transmitter
):
    transmitter = transmitter_satellite
    receiver = receiver_satellite
    cmd_tx, cmd_rx = cmd_transmitter

    # Initialize
    cmd_tx.request_get_response(
        "initialize", {"_bor_timeout": 1, "_data_timeout": 1, "_eor_timeout": 1, "_payload_threshold": 0}
    )
    cmd_rx.request_get_response("initialize", {"_eor_timeout": 1})
    wait_for_state(transmitter.fsm, "INIT", 1)
    wait_for_state(receiver.fsm, "INIT", 1)

    # Launch
    cmd_tx.request_get_response("launch")
    cmd_rx.request_get_response("launch")
    wait_for_state(transmitter.fsm, "ORBIT", 1)
    wait_for_state(receiver.fsm, "ORBIT", 1)

    # Start receiver
    cmd_rx.request_get_response("start", "test_run_1")
    wait_for_state(receiver.fsm, "RUN")

    # Add sender
    tx_service = DiscoveredService(get_uuid("mock_transmitter"), CHIRPServiceIdentifier.DATA, "127.0.0.1", DATA_PORT)
    receiver._add_sender_callback(tx_service)

    # Start transmitter
    cmd_tx.request_get_response("start", "test_run_1")
    wait_for_state(transmitter.fsm, "RUN")

    # Wait until BOR is received
    timeout = 1.0
    while len(receiver.last_bors) < 1 and timeout > 0:
        time.sleep(0.05)
        timeout -= 0.05
    assert len(receiver.last_bors) == 1, "BOR not received"

    # Check BOR
    bor_rx_sender, bor_rx_tags, bor_rx_config = receiver.last_bors[-1]
    assert bor_rx_tags == {"dummy": True}
    assert bor_rx_config["_payload_threshold"] == 0

    # Send data
    data = b"123"
    data_record = transmitter.new_data_record()
    data_record.add_block(data)
    transmitter.send_data_record(data_record)

    # Wait until data record is received
    timeout = 1.0
    while len(receiver.last_data_records) < 1 and timeout > 0:
        time.sleep(0.05)
        timeout -= 0.05
    assert len(receiver.last_data_records) == 1, "Data not received"

    # Check data record
    data_rx_sender, data_rx_data_record = receiver.last_data_records[-1]
    assert len(data_rx_data_record.blocks) == 1
    assert data_rx_data_record.blocks[0] == data

    # Some calls for transmitter coverage
    transmitter.mark_run_tainted()
    assert transmitter.can_send_record()

    # Stop transmitter
    cmd_tx.request_get_response("stop")
    wait_for_state(transmitter.fsm, "ORBIT")

    # Wait until EOR is received
    timeout = 1.0
    while len(receiver.last_eors) < 1 and timeout > 0:
        time.sleep(0.05)
        timeout -= 0.05
    assert len(receiver.last_eors) == 1, "EOR not received"

    # Check EOR
    eor_exp_run_condition = RunCondition.TAINTED
    eor_rx_sender, eor_rx_tags, eor_rx_meta = receiver.last_eors[-1]
    assert eor_rx_tags == {"data_collected": False}
    assert eor_rx_meta["run_id"] == "test_run_1"
    assert eor_rx_meta["condition_code"] == eor_exp_run_condition.value
    assert eor_rx_meta["condition"] == eor_exp_run_condition.name

    # Some calls for receiver coverage
    tx_service.alive = False
    receiver._add_sender_callback(tx_service)
    assert receiver.data_transmitters is None

    # Stop receiver
    cmd_rx.request_get_response("stop")
    wait_for_state(receiver.fsm, "ORBIT")

    # Land
    cmd_tx.request_get_response("land")
    cmd_rx.request_get_response("land")
    wait_for_state(transmitter.fsm, "INIT")
    wait_for_state(receiver.fsm, "INIT")


def test_data_satellites_transmitter_failure(
    transmitter_satellite: DummyTransmitterSatellite, receiver_satellite: DummyReceiverSatellite, cmd_transmitter
):
    transmitter = transmitter_satellite
    receiver = receiver_satellite
    cmd_tx, cmd_rx = cmd_transmitter

    # Set transmitter to throw in RUN
    transmitter.throw_run = True

    # Initialize
    cmd_tx.request_get_response(
        "initialize", {"_bor_timeout": 1, "_data_timeout": 1, "_eor_timeout": 1, "_payload_threshold": 0}
    )
    cmd_rx.request_get_response("initialize", {"_eor_timeout": 1})
    wait_for_state(transmitter.fsm, "INIT", 1)
    wait_for_state(receiver.fsm, "INIT", 1)

    # Launch
    cmd_tx.request_get_response("launch")
    cmd_rx.request_get_response("launch")
    wait_for_state(transmitter.fsm, "ORBIT", 1)
    wait_for_state(receiver.fsm, "ORBIT", 1)

    # Start receiver
    cmd_rx.request_get_response("start", "test_run_1")
    wait_for_state(receiver.fsm, "RUN")

    # Add sender
    tx_service = DiscoveredService(get_uuid("mock_transmitter"), CHIRPServiceIdentifier.DATA, "127.0.0.1", DATA_PORT)
    receiver._add_sender_callback(tx_service)

    # Start transmitter
    cmd_tx.request_get_response("start", "test_run_1")
    wait_for_state(transmitter.fsm, "RUN")

    # Wait until BOR is received
    timeout = 1.0
    while len(receiver.last_bors) < 1 and timeout > 0:
        time.sleep(0.05)
        timeout -= 0.05
    assert len(receiver.last_bors) == 1, "BOR not received"

    # Wait until receiver in SAFE
    wait_for_state(transmitter.fsm, "ERROR")
    wait_for_state(receiver.fsm, "SAFE")

    # Check EOR
    assert len(receiver.last_eors) == 1, "EOR not received"
    eor_exp_run_condition = RunCondition.TAINTED | RunCondition.ABORTED
    eor_rx_sender, eor_rx_tags, eor_rx_meta = receiver.last_eors[-1]
    assert eor_rx_meta["condition_code"] == eor_exp_run_condition.value
    assert eor_rx_meta["condition"] == eor_exp_run_condition.name


def test_data_satellites_receiver_failure(
    transmitter_satellite: DummyTransmitterSatellite, receiver_satellite: DummyReceiverSatellite, cmd_transmitter
):
    transmitter = transmitter_satellite
    receiver = receiver_satellite
    cmd_tx, cmd_rx = cmd_transmitter

    # Set receiver to throw in RUN
    receiver.throw_run = True

    # Initialize
    cmd_tx.request_get_response(
        "initialize", {"_bor_timeout": 1, "_data_timeout": 1, "_eor_timeout": 1, "_payload_threshold": 0}
    )
    cmd_rx.request_get_response("initialize", {"_eor_timeout": 1})
    wait_for_state(transmitter.fsm, "INIT", 1)
    wait_for_state(receiver.fsm, "INIT", 1)

    # Launch
    cmd_tx.request_get_response("launch")
    cmd_rx.request_get_response("launch")
    wait_for_state(transmitter.fsm, "ORBIT", 1)
    wait_for_state(receiver.fsm, "ORBIT", 1)

    # Start receiver
    cmd_rx.request_get_response("start", "test_run_1")
    wait_for_state(receiver.fsm, "RUN")

    # Add sender
    tx_service = DiscoveredService(get_uuid("mock_transmitter"), CHIRPServiceIdentifier.DATA, "127.0.0.1", DATA_PORT)
    receiver._add_sender_callback(tx_service)

    # Start transmitter
    cmd_tx.request_get_response("start", "test_run_1")
    wait_for_state(transmitter.fsm, "RUN")

    # Wait until transmitter in SAFE or ERROR
    wait_for_state(receiver.fsm, "ERROR")
    timeout = 2.0
    while transmitter.fsm.current_state_value not in [SatelliteState.SAFE, SatelliteState.ERROR] and timeout > 0:
        time.sleep(0.05)
        timeout -= 0.05
    assert transmitter.fsm.current_state_value in [SatelliteState.SAFE, SatelliteState.ERROR]
