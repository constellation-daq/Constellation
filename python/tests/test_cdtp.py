#!/usr/bin/env python3
"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0
"""

import threading
import time
from unittest.mock import MagicMock, patch

import pytest
from conftest import mocket
from constellation.cdtp import CDTPMessageIdentifier, DataTransmitter
from constellation.cscp import CommandTransmitter
from constellation.datareceiver import DataReceiver
from constellation.datasender import DataSender

DATA_PORT = 50101
CMD_PORT = 10101


@pytest.fixture
def mock_data_transmitter(mock_socket_sender: mocket):
    mock_socket_sender.port = DATA_PORT
    t = DataTransmitter("mock_sender", mock_socket_sender)
    yield t


@pytest.fixture
def mock_data_receiver(mock_socket_receiver: mocket):
    mock_socket_receiver.port = DATA_PORT
    r = DataTransmitter("mock_receiver", mock_socket_receiver)
    yield r


@pytest.fixture
def mock_sender_satellite(mock_chirp_socket):
    """Mock a Satellite for a specific device, ie. a class inheriting from Satellite."""

    def mocket_factory(*args, **kwargs):
        m = mocket()
        m.endpoint = 1
        return m

    class MockSenderSatellite(DataSender):
        def do_run(self, payload: any):
            self.payload_id = 0
            while self.payload_id < 10:
                self.data_queue.put((f"mock payload {self.payload_id}", {}))
                self.payload_id += 1
                time.sleep(0.5)
            return "Send finished"

    with patch("constellation.base.zmq.Context") as mock:
        mock_context = MagicMock()
        mock_context.socket = mocket_factory
        mock.return_value = mock_context
        s = MockSenderSatellite(
            name="mydevice1",
            group="mockstellation",
            cmd_port=CMD_PORT,
            mon_port=22222,
            hb_port=33333,
            data_port=DATA_PORT,
            interface="127.0.0.1",
        )
        t = threading.Thread(target=s.run_satellite)
        t.start()
        # give the threads a chance to start
        time.sleep(0.1)
        yield s


@pytest.fixture
def mock_receiver_satellite(mock_socket_sender: mocket, mock_socket_receiver: mocket):
    """Mock a Satellite for a specific device, ie. a class inheriting from Satellite."""

    def mocket_factory(*args, **kwargs):
        m = mocket()
        m.endpoint = 1
        return m

    class MockReceiverSatellite(DataReceiver):
        def do_initializing(self, payload: any) -> str:
            self.mock_directory = []
            return super().do_initializing(payload)

        def do_run(self):
            self.BOR = True
            self.EOR = False
            mock_file = []
            while not self._state_thread_evt.is_set():
                msg = self.data_queue.get(block=True, timeout=0.5)
                if msg.msgtype == CDTPMessageIdentifier.BOR:
                    self.BOR = False

                elif msg.msgtype == CDTPMessageIdentifier.EOR:
                    self.EOR = True
                mock_file.append(msg)
            self.mock_directory.append(mock_file)

    with patch("constellation.base.zmq.Context") as mock:
        mock_context = MagicMock()
        mock_context.socket = mocket_factory
        mock.return_value = mock_context
        s = MockReceiverSatellite(
            "mock_receiver",
            "mockstellation",
            cmd_port=CMD_PORT,
            mon_port=22222,
            hb_port=33333,
            interface="127.0.0.1",
        )
        t = threading.Thread(target=s.run_satellite)
        t.start()
        # give the threads a chance to start
        time.sleep(0.1)
        yield s


@pytest.mark.forked
def test_datatransmitter(
    mock_data_transmitter: DataTransmitter, mock_data_receiver: DataTransmitter
):
    sender = mock_data_transmitter
    rx = mock_data_receiver

    sender.send_start("mock payload")
    msg = rx.recv()

    assert msg.payload == "mock payload"
    assert msg.msgtype == CDTPMessageIdentifier.BOR


@pytest.mark.forked
def test_sending_package(
    mock_sender_satellite,
    mock_data_receiver,
):
    mock = mocket()
    mock.return_value = mock
    mock.endpoint = 0
    mock.port = CMD_PORT

    commander = CommandTransmitter("cmd", mock)
    transmitter = mock_sender_satellite
    payloads = []
    rx = mock_data_receiver

    commander.send_request("initialize", {"mock key": "mock argument string"})
    time.sleep(0.5)
    commander.send_request("launch")
    time.sleep(0.5)
    commander.send_request("start")
    time.sleep(0.5)
    assert (
        transmitter.fsm.current_state.id == "RUN"
    ), "Could not set up test environment"

    msgs = []
    BOR = True
    for idx in range(11):
        msg = rx.recv()
        msgs.append(msg)
        if BOR:
            assert msg.msgtype == CDTPMessageIdentifier.BOR
            assert msg.payload != f"mock payload {idx}"
            BOR = False
        else:
            assert msg.msgtype == CDTPMessageIdentifier.DAT
            assert msg.payload == f"mock payload {idx-1}"
            # assert msg.sequence_number == idx, "Sequence number not expected order"
            # assert msg.name == "mock sender"

        payloads.append(msg.payload)
    msg = rx.recv()
    assert msg.msgtype == CDTPMessageIdentifier.EOR
    assert transmitter.payload_id == 10


@pytest.mark.forked
def test_receiving_package(
    mock_data_transmitter: DataTransmitter,
    mock_receiver_satellite,
):
    mock = mocket()
    mock.return_value = mock
    mock.endpoint = 0
    mock.port = CMD_PORT
    commander = CommandTransmitter("cmd", mock)

    receiver = mock_receiver_satellite
    tx = mock_data_transmitter
    commander.send_request("initialize", {"mock key": "mock argument string"})
    time.sleep(0.5)
    commander.send_request("launch")
    time.sleep(0.5)
    commander.send_request("start")
    time.sleep(0.5)

    assert receiver.fsm.current_state.id == "RUN", "Could not set up test environment"
    payload = "mock payload"
    tx.send_start(payload)
    tx.send_data(payload)
    tx.send_data(payload)
    tx.send_end(payload)
    assert len(receiver.mock_directory[0]) == 4
    for idx in range(4):
        assert receiver.mock_directory[0][idx] == "mock payload"
    assert receiver.mock_directory


"""
@pytest.mark.forked
def test_datareceiver(
    mock_cmd_transmitter: CommandTransmitter, mock_data_transmitter: DataTransmitter
):
    receiver = H5DataReceiverWriter(
        name="mock_name",
        group="mock_group",
        cmd_port=71115,
        hb_port=71116,
        mon_port=71117,
        interface="*",
    )
    commander = mock_cmd_transmitter
    sender = mock_data_transmitter
    sat_thread = threading.Thread(target=receiver.run_satellite())
    sat_thread.start()

    # NOTE: Temporary solution for getting config-files
    write_config = {}

    for category in ["constellation", "satellites"]:
        write_config.update(
            get_config(
                config_path="python/constellation/configs/example.toml",
                category=category,
                host_class="H5DataReceiverWriter",
            )
        )
    commander.send_request("initialize", write_config)
    time.sleep(0.5)
    commander.send_request("launch")
    time.sleep(0.5)
    commander.send_request("start")

    sender.send_start()
    for _ in range(10):
        sender.send_data([1, 2, 3])
"""
