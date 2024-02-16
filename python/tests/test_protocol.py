#!/usr/bin/env python3

import pytest
import uuid

from unittest.mock import patch

from constellation.protocol import (
    CHIRPBeaconTransmitter,
    CHIRPServiceIdentifier,
    CHIRPMessageType,
)


def test_chirp_beacon_send_recv():
    sender = CHIRPBeaconTransmitter(
        uuid.uuid5(uuid.NAMESPACE_DNS, "mock_sender"),
        uuid.uuid5(uuid.NAMESPACE_DNS, "mockstellation"),
    )
    receiver = CHIRPBeaconTransmitter(
        uuid.uuid5(uuid.NAMESPACE_DNS, "mock_receiver"),
        uuid.uuid5(uuid.NAMESPACE_DNS, "mockstellation"),
    )
    sender.broadcast_service(CHIRPServiceIdentifier.DATA, CHIRPMessageType.OFFER, 666)
    res = receiver.listen()
    assert (
        res.serviceid == CHIRPServiceIdentifier.DATA
    ), "Receiving chirp package failed."

    # listen a second time
    res = receiver.listen()
    assert not res, "Non-blocking receive w/o sending data failed."

    # receive two packages
    sender.broadcast_service(CHIRPServiceIdentifier.DATA, CHIRPMessageType.OFFER, 666)
    sender.broadcast_service(CHIRPServiceIdentifier.DATA, CHIRPMessageType.OFFER, 666)
    res = receiver.listen()
    assert (
        res.serviceid == CHIRPServiceIdentifier.DATA
    ), "First of two packages went missing"
    res = receiver.listen()
    assert (
        res.serviceid == CHIRPServiceIdentifier.DATA
    ), "Second of two packages went missing"
    res = receiver.listen()
    assert not res, "Non-blocking receive w/o sending data failed."

    # send package, then let the test for header fail
    sender.broadcast_service(CHIRPServiceIdentifier.DATA, CHIRPMessageType.OFFER, 666)
    with patch("constellation.protocol.CHIRP_HEADER"):
        with pytest.raises(RuntimeError) as e:
            res = receiver.listen()
        assert "malformed CHIRP header" in str(
            e.value
        ), "Wrong chirp header did not trigger expected exception message."
