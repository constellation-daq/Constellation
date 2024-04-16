#!/usr/bin/env python3
"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0
"""

import os
import pathlib
import threading
import time
from tempfile import TemporaryDirectory
from unittest.mock import MagicMock, patch

import h5py
import pytest
from conftest import mocket
from constellation.broadcastmanager import DiscoveredService
from constellation.cdtp import CDTPMessageIdentifier, DataTransmitter
from constellation.chirp import CHIRPServiceIdentifier, get_uuid
from constellation.cscp import CommandTransmitter
from constellation.datareceiver import H5DataReceiverWriter
from constellation.datasender import DataSender

DATA_PORT = 50101
CMD_PORT = 10101
FILE_NAME = "mock_file.h5"


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

    with patch("constellation.core.base.zmq.Context") as mock:
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
        m.endpoint = 0
        return m

    class MockReceiverSatellite(H5DataReceiverWriter):
        def __init__(self, temp_dir, *args, **kwargs):
            super().__init__(*args, **kwargs)
            self.temp_dir = temp_dir

        def do_initializing(self, payload: any) -> str:
            super().do_initializing(payload)
            self.directroy_name = self.temp_dir
            self.file_name_pattern = FILE_NAME
            return "Initializing"

    with patch("constellation.core.base.zmq.Context") as mock:
        mock_context = MagicMock()
        mock_context.socket = mocket_factory
        mock.return_value = mock_context
        with TemporaryDirectory() as tmpdirname:
            s = MockReceiverSatellite(
                name="mock_receiver",
                group="mockstellation",
                cmd_port=CMD_PORT,
                mon_port=22222,
                hb_port=33333,
                interface="127.0.0.1",
                temp_dir=tmpdirname,
            )
            t = threading.Thread(target=s.run_satellite)
            t.start()
            # give the threads a chance to start
            time.sleep(0.1)
            yield s, tmpdirname


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
def test_receive_writing_package(
    mock_data_transmitter: DataTransmitter,
    mock_receiver_satellite,
):
    mock = mocket()
    mock.return_value = mock
    mock.endpoint = 1
    mock.port = CMD_PORT
    commander = CommandTransmitter("cmd", mock)
    service = DiscoveredService(
        get_uuid("mock_sender"),
        CHIRPServiceIdentifier.DATA,
        "127.0.0.1",
        port=DATA_PORT,
    )

    receiver, tmpdir = mock_receiver_satellite
    tx = mock_data_transmitter
    commander.send_request("initialize", {"mock key": "mock argument string"})
    time.sleep(0.5)
    receiver._add_sender(service)

    payload = [1234]
    tx.send_start(["mock_start"])
    tx.send_data(payload)
    tx.send_data(payload)
    tx.send_end(["mock_end"])

    commander.send_request("launch")
    time.sleep(0.5)
    assert receiver.run_number == 0
    assert receiver.data_queue.qsize() == 4, "Could not receive all data packets."
    commander.send_request("start")
    time.sleep(1)
    assert receiver.fsm.current_state.id == "RUN", "Could not set up test environment"
    commander.send_request("stop")
    time.sleep(0.5)
    assert receiver.run_number == 1

    assert os.path.exists(os.path.join(tmpdir, FILE_NAME))
    h5file = h5py.File(tmpdir / pathlib.Path(FILE_NAME))
    assert "mock_sender" in h5file.keys()
    assert "BOR_1" in h5file["mock_sender"].keys()
    assert "mock_start" in str(h5file["mock_sender"]["BOR_1"][0], encoding="utf-8")
    assert "EOR_1" in h5file["mock_sender"].keys()
    assert "mock_end" in str(h5file["mock_sender"]["EOR_1"][0], encoding="utf-8")
    assert "data_run_1" in h5file["mock_sender"].keys()
    assert payload == h5file["mock_sender"]["data_run_1"][0]
    assert payload == h5file["mock_sender"]["data_run_1"][1]
