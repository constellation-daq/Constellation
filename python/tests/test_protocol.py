#!/usr/bin/env python3

import pytest

from unittest.mock import patch

from constellation.protocol import SatelliteBeacon, ServiceIdentifier, MessageType


def test_chirp_beacon_send_recv():
    sender = SatelliteBeacon("mock_sender", "mockstellation")
    receiver = SatelliteBeacon("mock_receiver", "mockstellation")
    sender.broadcast_service(ServiceIdentifier.DATA, MessageType.OFFER, 666)
    res = receiver.listen()
    assert res[3] == ServiceIdentifier.DATA, "Receiving chirp package failed."

    # listen a second time
    res = receiver.listen()
    assert not res, "Non-blocking receive w/o sending data failed."

    # receive two packages
    sender.broadcast_service(ServiceIdentifier.DATA, MessageType.OFFER, 666)
    sender.broadcast_service(ServiceIdentifier.DATA, MessageType.OFFER, 666)
    res = receiver.listen()
    assert res[3] == ServiceIdentifier.DATA, "First of two packages missing"
    res = receiver.listen()
    assert res[3] == ServiceIdentifier.DATA, "Second of two packages missing"
    res = receiver.listen()
    assert not res, "Non-blocking receive w/o sending data failed."

    # send package, then let the test for header fail
    sender.broadcast_service(ServiceIdentifier.DATA, MessageType.OFFER, 666)
    with patch("constellation.protocol.CHIRP_HEADER"):
        with pytest.raises(Exception) as e:
            res = receiver.listen()
        assert "malformed CHIRP header" in str(
            e.value
        ), "Wrong chirp header did not trigger expected exception message."
