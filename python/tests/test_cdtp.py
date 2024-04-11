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
from constellation.satellite import Satellite


@pytest.fixture
def mock_sender_satellite(mock_socket_sender, mock_socket_receiver):
    """Mock a Satellite for a specific device, ie. a class inheriting from Satellite."""

    def mocket_factory(*args, **kwargs):
        m = mocket()
        return m

    class MockSenderSatellite(Satellite):
        def do_initializing(self, payload):
            self.transmitter = DataTransmitter("mock transmitter", mock_socket_sender)
            return "finished with mock initialization"

        def do_run(self):
            self.payload_id = 0
            self.transmitter.send_data(
                "mock payload",
                CDTPMessageIdentifier.BOR,
            )
            self.payload_id += 1
            while self.payload_id < 10:
                self.transmitter.send_data("mock payload", mock_socket_receiver)
                self.payload_id += 1

            self.transmitter.send_data("mock payload", CDTPMessageIdentifier.EOR)
            self.payload_id += 1

    with patch("constellation.base.zmq.Context") as mock:
        mock_context = MagicMock()
        mock_context.socket = mocket_factory
        mock.return_value = mock_context
        s = MockSenderSatellite("mock_sender", "mockstellation", 11111, 22222, 33333)
        t = threading.Thread(target=s.run_satellite)
        t.start()
        # give the threads a chance to start
        time.sleep(0.1)
        yield s


@pytest.fixture
def mock_receiver_satellite(mock_socket_sender, mock_socket_receiver):
    """Mock a Satellite for a specific device, ie. a class inheriting from Satellite."""

    def mocket_factory(*args, **kwargs):
        m = mocket()
        return m

    class MockReceiverSatellite(Satellite):
        def do_initializing(self, payload):
            self.mock_directory = []
            self.receiver = DataTransmitter("mock receiver", mock_socket_receiver)
            return "finished with mock initialization"

        def do_run(self):
            BOR = True
            EOR = False
            mock_file = []
            while not EOR:
                payload, run_seq, run_id, host, ts = self.receiver.recv(
                    mock_socket_sender
                )

                if run_seq == CDTPMessageIdentifier.BOR:
                    BOR = False
                    continue

                if run_seq == CDTPMessageIdentifier.EOR:
                    EOR = True
                    continue

                if not BOR and run_seq == CDTPMessageIdentifier.DAT:
                    mock_file.append(payload)
            self.mock_directory.append(mock_file)

    with patch("constellation.base.zmq.Context") as mock:
        mock_context = MagicMock()
        mock_context.socket = mocket_factory
        mock.return_value = mock_context
        s = MockReceiverSatellite(
            "mock_receiver", "mockstellation", 11111, 22222, 33333
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


def test_sending_package(
    mock_sender_satellite,
    mock_receiver_satellite,
    mock_cmd_transmitter,
    mock_data_receiver,
    mock_socket_sender,
):
    commander = mock_cmd_transmitter
    transmitter = mock_sender_satellite
    payloads = []
    rx = mock_data_receiver
    commander.send_request("initialize", "mock argument string")
    commander.send_request("launch")
    commander.send_request("start")

    BOR = True
    for idx in range(12):
        payload, run_seq, run_id, host, ts = rx.recv(mock_socket_sender)
        assert payload == "mock payload"
        assert run_id == idx
        assert host == "mock sender"

        if BOR:
            assert run_seq == CDTPMessageIdentifier.BOR
            BOR = False

        payloads.append(payload)
    assert run_seq == CDTPMessageIdentifier.EOR
    assert transmitter.payload_id == 12


def test_receiving_package(
    mock_data_transmitter,
    mock_receiver_satellite,
    mock_cmd_transmitter,
    mock_socket_receiver,
):
    commander = mock_cmd_transmitter
    receiver = mock_receiver_satellite
    tx = mock_data_transmitter
    commander.send_request("initialize", "mock argument string")
    commander.send_request("launch")
    commander.send_request("start")
    payload = 1234
    tx.send_data(payload, CDTPMessageIdentifier.BOR, socket=mock_socket_receiver)
    tx.send_data(payload, CDTPMessageIdentifier.DAT, socket=mock_socket_receiver)
    tx.send_data(payload, CDTPMessageIdentifier.EOR, socket=mock_socket_receiver)
    assert len(receiver.mock_directory[0]) == 3
    for idx in range(3):
        assert receiver.mock_directory[0][idx] == payload
