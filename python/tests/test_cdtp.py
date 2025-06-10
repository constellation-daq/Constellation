"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2
"""

import os
import pathlib
import random
import threading
import time
from tempfile import TemporaryDirectory
from typing import Any
from unittest.mock import MagicMock, patch

import h5py
import numpy as np
import pytest
import zmq
from conftest import mocket, wait_for_state

from constellation.core import __version__
from constellation.core.broadcastmanager import DiscoveredService
from constellation.core.cdtp import CDTPMessageIdentifier, DataTransmitter
from constellation.core.chirp import CHIRPServiceIdentifier, get_uuid
from constellation.core.cscp import CommandTransmitter
from constellation.core.datasender import DataSender
from constellation.core.network import get_loopback_interface_name
from constellation.satellites.H5DataWriter.H5DataWriter import H5DataWriter

DATA_PORT = 50101
MON_PORT = 22222
CMD_PORT = 10101
FILE_NAME = "mock_file_{run_identifier}.h5"


@pytest.fixture
def mock_data_transmitter(mock_socket_sender: mocket):
    mock_socket_sender.port = DATA_PORT
    t = DataTransmitter("mock_sender", mock_socket_sender)
    yield t


@pytest.fixture
def data_transmitter():
    ctx = zmq.Context()
    socket = ctx.socket(zmq.PUSH)
    socket.bind(f"tcp://127.0.0.1:{DATA_PORT}")
    t = DataTransmitter("simple_sender", socket)
    yield t


@pytest.fixture
def commander():
    ctx = zmq.Context()
    socket = ctx.socket(zmq.REQ)
    socket.connect(f"tcp://127.0.0.1:{CMD_PORT}")
    commander = CommandTransmitter("cmd", socket)
    yield commander


@pytest.fixture
def mock_data_receiver(mock_socket_receiver: mocket):
    mock_socket_receiver.port = DATA_PORT
    r = DataTransmitter("mock_receiver", mock_socket_receiver)
    yield r


@pytest.fixture
def data_receiver():
    ctx = zmq.Context()
    socket = ctx.socket(zmq.PULL)
    socket.connect(f"tcp://127.0.0.1:{DATA_PORT}")
    r = DataTransmitter("simple_receiver", socket)
    yield r


@pytest.fixture
def mock_sender_satellite(mock_chirp_transmitter):
    """Mock a Satellite for a specific device, ie. a class inheriting from Satellite."""

    def mocket_factory(*args, **kwargs):
        m = mocket()
        m.endpoint = 1
        return m

    class MockSenderSatellite(DataSender):
        def do_initializing(self, config):
            self.BOR = {"status": "set at initialization"}
            return "done"

        def do_starting(self, payload: Any):
            self.BOR = "set in do_starting()"
            return "done"

        def do_run(self, payload: Any):
            self.payload_id = 0
            while self.payload_id < 10:
                payload = f"mock payload {self.payload_id}".encode("utf-8")
                self.data_queue.put((payload, {}))
                self.payload_id += 1
                time.sleep(0.02)
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
            interface=[get_loopback_interface_name()],
        )
        t = threading.Thread(target=s.run_satellite)
        t.start()
        # give the threads a chance to start
        time.sleep(0.1)
        yield s


@pytest.fixture
def receiver_satellite():
    """A receiver Satellite."""

    class MockReceiverSatellite(H5DataWriter):
        def do_initializing(self, config: dict[str, Any]) -> str:
            res = super().do_initializing(config)
            # configure monitoring with higher frequency
            self._configure_monitoring(0.1)
            return res

    s = MockReceiverSatellite(
        name="mock_receiver",
        group="mockstellation",
        cmd_port=CMD_PORT,
        mon_port=MON_PORT,
        hb_port=33333,
        interface=[get_loopback_interface_name()],
    )
    t = threading.Thread(target=s.run_satellite)
    t.start()
    # give the threads a chance to start
    time.sleep(0.2)
    yield s


@pytest.fixture
def sender_satellite():
    """A sender Satellite."""

    class MockSenderSatellite(DataSender):
        def do_run(self, payload: any):
            self.payload_id = 0
            while self.payload_id < 10 and not self._state_thread_evt.is_set():
                payload = f"mock payload {self.payload_id}".encode("utf-8")
                self.data_queue.put((payload, {}))
                self.payload_id += 1
                time.sleep(0.02)
            while not self._state_thread_evt.is_set():
                time.sleep(0.02)
            return "Send finished"

    s = MockSenderSatellite(
        name="mock_sender",
        group="mockstellation",
        cmd_port=CMD_PORT,
        mon_port=MON_PORT,
        data_port=DATA_PORT,
        hb_port=33333,
        interface=[get_loopback_interface_name()],
    )
    t = threading.Thread(target=s.run_satellite)
    t.start()
    # give the threads a chance to start
    time.sleep(0.2)
    yield s


@pytest.mark.forked
def test_datatransmitter(mock_data_transmitter: DataTransmitter, mock_data_receiver: DataTransmitter):
    sender = mock_data_transmitter
    rx = mock_data_receiver

    # string
    payload = "mock payload"
    sender.send_start(payload)
    msg = rx.recv()
    assert msg is not None
    assert msg.payload == payload
    assert msg.msgtype == CDTPMessageIdentifier.BOR

    # simple list
    payload = ["mock payload", "more mock data"]
    sender.send_data(payload)
    msg = rx.recv()
    assert msg is not None
    assert msg.payload == payload
    assert msg.msgtype == CDTPMessageIdentifier.DAT

    # bytes
    payload = np.arange(0, 1000).tobytes()
    sender.send_data(payload)
    msg = rx.recv()
    assert msg is not None
    assert msg.payload == payload
    assert msg.msgtype == CDTPMessageIdentifier.DAT

    # multi-frame bytes
    payload = [np.arange(0, 1000).tobytes(), np.arange(10000, 20000).tobytes()]
    sender.send_data(payload)
    msg = rx.recv()
    assert msg is not None
    assert msg.payload == payload
    assert msg.msgtype == CDTPMessageIdentifier.DAT

    payload = None
    sender.send_data(payload)
    msg = rx.recv()
    assert msg is not None
    assert msg.payload == payload
    assert msg.msgtype == CDTPMessageIdentifier.DAT

    payload = {"mock": True}
    sender.send_end(payload)
    msg = rx.recv()
    assert msg is not None
    assert msg.payload == payload
    assert msg.msgtype == CDTPMessageIdentifier.EOR


@pytest.mark.forked
def test_sending_package(
    mock_sender_satellite,
    mock_data_receiver,
):
    mock = mocket()
    mock.endpoint = 0
    mock.port = CMD_PORT

    commander = CommandTransmitter("cmd", mock)
    transmitter = mock_sender_satellite
    rx = mock_data_receiver

    assert not transmitter.BOR
    commander.send_request("initialize", {"mock key": "mock argument string"})
    wait_for_state(transmitter.fsm, "INIT")
    assert transmitter.BOR == {"status": "set at initialization"}
    commander.send_request("launch")
    wait_for_state(transmitter.fsm, "ORBIT")
    commander.send_request("start", "100102")
    wait_for_state(transmitter.fsm, "RUN")
    assert transmitter.fsm.current_state_value.name == "RUN", "Could not set up test environment"

    BOR = True
    for idx in range(11):
        msg = rx.recv()
        if BOR:
            assert msg.msgtype == CDTPMessageIdentifier.BOR
            assert msg.payload == "set in do_starting()"
            BOR = False
        else:
            assert msg.msgtype == CDTPMessageIdentifier.DAT
            assert msg.payload.decode("utf-8") == f"mock payload {idx - 1}"
            # assert msg.sequence_number == idx, "Sequence number not expected order"
            # assert msg.name == "mock sender"

    time.sleep(0.2)
    # still in RUN?
    assert transmitter.fsm.current_state_value.name == "RUN", "Ended RUN state early"
    # go to stop to send EOR
    commander.send_request("stop", "")
    wait_for_state(transmitter.fsm, "ORBIT")
    # EOR
    msg = rx.recv()
    assert msg.msgtype == CDTPMessageIdentifier.EOR
    assert transmitter.payload_id == 10


@pytest.mark.forked
def test_receive_writing_package(
    receiver_satellite,
    data_transmitter,
    commander,
):
    """Test receiving and writing data, verify state machine of DataSender."""
    service = DiscoveredService(
        get_uuid("simple_sender"),
        CHIRPServiceIdentifier.DATA,
        "127.0.0.1",
        port=DATA_PORT,
    )

    receiver = receiver_satellite
    tx = data_transmitter
    with TemporaryDirectory() as tmpdir:
        commander.request_get_response("initialize", {"_file_name_pattern": FILE_NAME, "_output_path": tmpdir})
        wait_for_state(receiver.fsm, "INIT", 1)
        receiver._add_sender(service)
        commander.request_get_response("launch")
        wait_for_state(receiver.fsm, "ORBIT", 1)

        payload = np.array(np.arange(1000), dtype=np.int16)
        assert receiver.run_identifier == ""

        for run_num in range(1, 4):
            # Send new data to handle
            tx.send_start({"mock_cfg": run_num, "other_val": "mockval"})
            # send once as byte array with and once w/o dtype
            tx.send_data(payload.tobytes(), {"dtype": f"{payload.dtype}"})
            tx.send_data(payload.tobytes())
            time.sleep(0.1)

            # Running satellite
            commander.request_get_response("start", str(run_num))
            wait_for_state(receiver.fsm, "RUN", 1)
            timeout = 0.5
            while not receiver.active_satellites or timeout < 0:
                time.sleep(0.05)
                timeout -= 0.05
            assert len(receiver.active_satellites) == 1, "No BOR received!"
            assert receiver.fsm.current_state_value.name == "RUN", "Could not set up test environment"
            commander.request_get_response("stop")
            time.sleep(0.5)
            # receiver should still be in 'stopping' as no EOR has been sent
            assert receiver.fsm.current_state_value.name == "stopping", "Receiver stopped before receiving EORE"
            # send EORE
            tx.send_end({"mock_end": f"whatanend{run_num}"})
            wait_for_state(receiver.fsm, "ORBIT", 1)
            assert receiver.run_identifier == str(run_num)

            # Does file exist and has it been written to?
            bor = "BOR"
            eor = "EOR"
            dat = [f"data_{run_num}_{i:09}" for i in range(1, 3)]

            fn = FILE_NAME.format(run_identifier=run_num)
            assert os.path.exists(os.path.join(tmpdir, fn))
            h5file = h5py.File(tmpdir / pathlib.Path(fn))
            assert h5file["MockReceiverSatellite.mock_receiver"]["swmr_mode"][()] == 0
            assert "simple_sender" in h5file.keys()
            assert bor in h5file["simple_sender"].keys()
            assert h5file["simple_sender"][bor]["mock_cfg"][()] == run_num
            assert eor in h5file["simple_sender"].keys()
            assert "whatanend" in str(h5file["simple_sender"][eor]["mock_end"][()], encoding="utf-8")
            assert set(dat).issubset(h5file["simple_sender"].keys()), "Data packets missing in file"
            assert (payload == h5file["simple_sender"][dat[0]]).all()
            # interpret the uint8 values again as uint16:
            assert (payload == np.array(h5file["simple_sender"][dat[1]]).view(np.uint16)).all()
            assert h5file["MockReceiverSatellite.mock_receiver"]["constellation_version"][()] == __version__.encode()
            h5file.close()


@pytest.mark.forked
def test_receive_writing_swmr_mode(
    receiver_satellite,
    data_transmitter,
    commander,
):
    """Test receiving and writing data, verify state machine of DataSender."""
    service = DiscoveredService(
        get_uuid("simple_sender"),
        CHIRPServiceIdentifier.DATA,
        "127.0.0.1",
        port=DATA_PORT,
    )

    receiver = receiver_satellite
    tx = data_transmitter
    with TemporaryDirectory() as tmpdir:
        commander.request_get_response(
            "initialize", {"_file_name_pattern": FILE_NAME, "_output_path": tmpdir, "allow_concurrent_reading": True}
        )
        wait_for_state(receiver.fsm, "INIT", 1)
        receiver._add_sender(service)
        commander.request_get_response("launch")
        wait_for_state(receiver.fsm, "ORBIT", 1)

        assert receiver.run_identifier == ""

        # check swmr status via cscp
        msg = commander.request_get_response("get_concurrent_reading_status")
        assert msg.verb_msg.lower().startswith("not")

        for run_num in range(1, 4):
            # Send new data to handle
            tx.send_start({"mock_cfg": run_num, "other_val": "mockval"})
            # send once as byte array with and once w/o dtype
            payload_sizes = []
            payloads = []
            for p in range(10):
                # send random-sized payloads
                psize = int(random.random() * 100000)
                payload = np.array(np.arange(psize), dtype=np.int16)
                if p % 2 == 0:
                    # send with meta info
                    tx.send_data(payload.tobytes(), {"dtype": f"{payload.dtype}", "moreinfo": "a very special payload"})
                else:
                    # send without meta info
                    tx.send_data(payload.tobytes())
                payload_sizes.append(psize)
                payloads.append(payload)
            time.sleep(0.1)

            assert receiver._swmr_mode_enabled is False
            # Running satellite
            commander.request_get_response("start", str(run_num))
            wait_for_state(receiver.fsm, "RUN", 1)
            timeout = 0.5
            while not receiver.active_satellites or timeout < 0:
                time.sleep(0.05)
                timeout -= 0.05
            assert len(receiver.active_satellites) == 1, "No BOR received!"
            assert receiver.fsm.current_state_value.name == "RUN", "Could not set up test environment"
            assert receiver._swmr_mode_enabled is True
            # check swmr status via cscp
            msg = commander.request_get_response("get_concurrent_reading_status")
            assert msg.verb_msg.lower().startswith("enabled")
            # stop
            commander.request_get_response("stop")
            # send EORE
            tx.send_end({"mock_end": f"whatanend{run_num}"})
            wait_for_state(receiver.fsm, "ORBIT", 1)
            assert receiver._swmr_mode_enabled is False
            assert receiver.run_identifier == str(run_num)

            # Does file exist and has it been written to?
            bor = "BOR"
            eor = "EOR"

            fn = FILE_NAME.format(run_identifier=run_num)
            assert os.path.exists(os.path.join(tmpdir, fn))
            h5file = h5py.File(tmpdir / pathlib.Path(fn))
            assert h5file["MockReceiverSatellite.mock_receiver"]["swmr_mode"][()] == 1
            assert "simple_sender" in h5file.keys()
            assert bor in h5file["simple_sender"].keys()
            assert h5file["simple_sender"][bor]["mock_cfg"][()] == run_num
            assert eor in h5file["simple_sender"].keys()
            assert "whatanend" in str(h5file["simple_sender"][eor][()], encoding="utf-8")
            assert "data" in h5file["simple_sender"].keys(), "Data missing in file"
            # interpret the uint8 values again as int16 and compare packets:
            for i, payload in enumerate(payloads):
                idx = h5file["simple_sender"]["data_idx"][i]
                if i > 0:
                    prev = h5file["simple_sender"]["data_idx"][i - 1]
                else:
                    prev = 0
                loaded = np.array(h5file["simple_sender"]["data"][prev:idx]).view(np.int16)
                assert (payload == loaded).all(), "Could not reconstruct correct payload values"
                # meta data
                idx = h5file["simple_sender"]["meta_idx"][i]
                if i > 0:
                    prev = h5file["simple_sender"]["meta_idx"][i - 1]
                else:
                    prev = 0
                loaded = str(h5file["simple_sender"]["meta"][prev:idx], encoding="utf-8")
                if i % 2 == 0:
                    assert "a very special payload" in loaded
                else:
                    assert loaded == "{}"
            # check that remainder of idx values is 0: (resized but unused)
            i += 1
            while i < h5file["simple_sender"]["data_idx"].shape[0]:
                assert h5file["simple_sender"]["data_idx"][i] == 0
                i += 1
            assert h5file["MockReceiverSatellite.mock_receiver"]["constellation_version"][()] == __version__.encode()
            h5file.close()


@pytest.mark.forked
def test_fail_sending_bor(commander, sender_satellite):
    """Test a run failing due to missing data receiver."""
    sender = sender_satellite
    # set timeout value for BOR to 1s
    commander.request_get_response("initialize", {"bor_timeout": 1000})
    wait_for_state(sender.fsm, "INIT", 1)
    commander.request_get_response("launch")
    wait_for_state(sender.fsm, "ORBIT", 1)
    commander.request_get_response("start", "no_receiver_run")
    # no receiver present, sending BOR should fail
    wait_for_state(sender.fsm, "ERROR", 2)


@pytest.mark.forked
def test_fail_sending_eor(commander, sender_satellite, data_receiver):
    """Test a run failing due to missing data receiver at EOR."""
    sender = sender_satellite
    rx = data_receiver
    # set timeout value for BOR to 1s
    commander.request_get_response("initialize", {"eor_timeout": 1000})
    wait_for_state(sender.fsm, "INIT", 1)
    commander.request_get_response("launch")
    wait_for_state(sender.fsm, "ORBIT", 1)
    commander.request_get_response("start", "no_receiver_at_end_of_run")
    msg = rx.recv()
    assert msg.msgtype == CDTPMessageIdentifier.BOR
    wait_for_state(sender.fsm, "RUN", 1)
    time.sleep(1)
    for i in range(10):
        msg = rx.recv()
        assert msg.msgtype == CDTPMessageIdentifier.DAT
    # close connection
    #
    # NOTE this will make sends fail with "zmq.error.Again: Resource temporarily
    # unavailable", NOT a timeout. As pytest does not support our exception
    # hooks, the pusher thread dies silently and the timeout waiting for the EOR
    # event will kick in.
    rx._socket.close()
    time.sleep(0.2)
    commander.request_get_response("stop")
    # no receiver active, sending EOR should fail
    wait_for_state(sender.fsm, "ERROR", 2)


@pytest.mark.forked
def test_receiver_stats(
    receiver_satellite,
    monitoringlistener,
    commander,
):
    """Test the stats sent by DataReceiver are received via CMDP."""
    dp = 23242
    ctx = zmq.Context()
    socket = ctx.socket(zmq.PUSH)
    socket.bind(f"tcp://127.0.0.1:{dp}")
    tx = DataTransmitter("simple_sender", socket)

    service = DiscoveredService(
        get_uuid("simple_sender"),
        CHIRPServiceIdentifier.DATA,
        "127.0.0.1",
        port=dp,
    )

    receiver = receiver_satellite
    ml, tmpdir = monitoringlistener
    commander.request_get_response("initialize", {"_file_name_pattern": FILE_NAME, "_output_path": tmpdir})
    wait_for_state(receiver.fsm, "INIT", 1)
    receiver._add_sender(service)
    commander.request_get_response("launch")
    wait_for_state(receiver.fsm, "ORBIT", 1)

    payload = np.array(np.arange(1000), dtype=np.int16)

    for run_num in range(1, 3):
        # Send new data to handle
        tx.send_start({"mock_cfg": 1, "other_val": "mockval"})
        # send once as byte array with and once w/o dtype
        tx.send_data(payload.tobytes(), {"dtype": f"{payload.dtype}"})
        tx.send_data(payload.tobytes())

        # Running satellite
        commander.request_get_response("start", str(run_num))
        wait_for_state(receiver.fsm, "RUN", 1)
        time.sleep(0.2)
        commander.request_get_response("stop")
        # send EOR
        tx.send_end({"mock_end": 22})
        wait_for_state(receiver.fsm, "ORBIT", 1)
    timeout = 4
    while timeout > 0 and not len(ml._metric_sockets) > 0:
        time.sleep(0.05)
        timeout -= 0.05
    assert len(receiver.receiver_stats) == 2
    assert len(receiver._metrics_callbacks) > 1
    assert len(ml._metric_sockets) == 1
    assert os.path.exists(os.path.join(tmpdir, "stats")), "Stats output directory not created"
    statfile = os.path.join(tmpdir, "stats", "MockReceiverSatellite.mock_receiver.nbytes.csv")
    timeout = 5
    while timeout > 0 and not os.path.exists(statfile):
        time.sleep(0.05)
        timeout -= 0.05
    assert os.path.exists(statfile), "Expected output metrics csv not found"
    # close thread and connections to allow temp dir to be removed
    ml._log_listening_shutdown()
    ml._metrics_listening_shutdown()
