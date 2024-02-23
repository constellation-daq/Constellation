"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0

BroadcastManger module provides classes for managing CHIRP broadcasts within
Constellation Satellites.
"""

import logging
import threading

import time
from uuid import UUID
from queue import Queue

from constellation.chirp import (
    CHIRPServiceIdentifier,
    CHIRPMessage,
    CHIRPMessageType,
    CHIRPBeaconTransmitter,
)


class DiscoveredService:
    """Class to hold discovered service data."""

    def __init__(
        self,
        host_uuid: UUID,
        serviceid: CHIRPServiceIdentifier,
        address,
        port: int,
        alive: bool = True,
    ):
        """Initialize variables."""
        self.host_uuid = host_uuid
        self.address = address
        self.port = port
        self.serviceid = serviceid
        self.alive = alive

    def __eq__(self, other):
        """Comparison operator for network-related properties."""
        return self.address == other.address and self.port == other.port

    def __str__(self):
        """Pretty-print a string for this service."""
        s = "Host {} offering service {} on {}:{} is alive: {}"
        return s.format(
            self.host_uuid, self.serviceid, self.address, self.port, self.alive
        )


class CHIRPBroadcastManager:
    """Manages service discovery and broadcast via the CHIRP protocol.

    Listening and reacting to CHIRP broadcasts is implemented in a dedicated
    thread that can be started after the class has been instantiated.

    Discovered services are added to a callback queue if a request for the
    service type has been added.

    Offered services can be registered and are announced on incoming request
    broadcasts.

    """

    def __init__(
        self,
        name: str,
        group: str,
        callback_queue: Queue,
    ):
        """Initialize parameters.

        :param host: Satellite name the BroadcastManager represents
        :type host: str
        :param group: group the Satellite belongs to
        :type group: str
        :param callback_queue: The queue in which discovery callbacks will be placed.
        :type callback_queue: Queue
        """
        self._stop_broadcasting = threading.Event()
        self._beacon = CHIRPBeaconTransmitter(name, group)

        # Keep registered callbacks for services
        self._chirp_callbacks = {}
        self._chirp_callbacks_q = callback_queue

        # Offered and discovered services
        self._registered_services = {}
        self.discovered_services = []
        self._chirp_thread = None

        # set up logging
        self._logger = logging.getLogger(name + ".broadcast")

    def start(self) -> None:
        """Start broadcast manager."""
        self._chirp_thread = threading.Thread(target=self._run, daemon=True)
        self._chirp_thread.start()

    def stop(self, depart: bool = True) -> None:
        """Indicate broadcast manager to stop."""
        if depart:
            self.broadcast_depart()
        self._stop_broadcasting.set()
        self._chirp_thread.join()

    def get_discovered(
        self, serviceid: CHIRPServiceIdentifier
    ) -> list[DiscoveredService]:
        """Return a list of already discovered services for a given identifier."""
        res = []
        for s in self.discovered_services:
            if s.serviceid == serviceid:
                res.append(s)
        return res

    def register_request(
        self, serviceid: CHIRPServiceIdentifier, callback: callable
    ) -> None:
        """Register new callback for ServiceIdentifier."""
        if serviceid in self._chirp_callbacks:
            self._logger.info("Overwriting callback")
        self._chirp_callbacks[serviceid] = callback
        # make a callback if a service has already been discovered
        for known in self.get_discovered(serviceid):
            self._chirp_callbacks_q.put((callback, known))

    def register_offer(self, serviceid: CHIRPServiceIdentifier, port: int) -> None:
        """Register new offered service or overwrite existing service."""
        if port in self._registered_services:
            self._logger.warning("Replacing service registration for port %d", port)
        self._registered_services[port] = serviceid

    def request(self, serviceid: CHIRPServiceIdentifier) -> None:
        """Request specific service.

        Should already have a registered callback via register_request(), or
        any incoming OFFERS will go unnoticed.

        """
        if serviceid not in self._chirp_callbacks:
            self._logger.warning(
                "Serviceid %s does not have a registered callback", serviceid
            )
        self._beacon.broadcast(serviceid, CHIRPMessageType.REQUEST)

    def broadcast_offers(self, serviceid: CHIRPServiceIdentifier = None) -> None:
        """Broadcast all registered services matching serviceid.

        Specify None for all registered services.

        """
        for port, sid in self._registered_services.items():
            if not serviceid or serviceid == sid:
                self._logger.debug("Broadcasting service OFFER on %d for %s", port, sid)
                self._beacon.broadcast(sid, CHIRPMessageType.OFFER, port)

    def broadcast_requests(self) -> None:
        """Broadcast all requests registered via register_request()."""
        for serviceid in self._chirp_callbacks:
            self._logger.debug("Broadcasting service REQUEST for %s", serviceid)
            self._beacon.broadcast(serviceid, CHIRPMessageType.REQUEST)

    def broadcast_depart(self) -> None:
        """Broadcast DEPART for all registered services."""
        for port, sid in self._registered_services.items():
            self._logger.debug("Broadcasting service DEPART on %d for %s", port, sid)
            self._beacon.broadcast(sid, CHIRPMessageType.DEPART, port)

    def _discover_service(self, msg: CHIRPMessage) -> None:
        """Add a service to internal list and possibly queue a callback."""
        service = DiscoveredService(
            msg.host_uuid, msg.serviceid, msg.from_address, msg.port
        )
        if service in self.discovered_services:
            self._logger.info(
                "Service already discovered: %s on host %s",
                msg.serviceid,
                msg.from_address,
            )
            # NOTE we might want to call the callback method for this service
            # anyway, in case this host was down (without sending DEPART) and is
            # now reconnecting. But then the bookkeeping has to be done higher up.
        else:
            # add service to internal list and queue callback (if registered)
            self._logger.info(
                "Received new OFFER for service: %s on host %s",
                msg.serviceid,
                msg.from_address,
            )
            try:
                callback = self._chirp_callbacks[msg.serviceid]
                self._chirp_callbacks_q.put((callback, service))
            except KeyError:
                self._logger.debug("No callback for service %s set up.", msg.serviceid)
            self.discovered_services.append(service)

    def _depart_service(self, msg: CHIRPMessage) -> None:
        """Depart with a service."""
        try:
            service = DiscoveredService(
                msg.host_uuid, msg.serviceid, msg.from_addr, msg.port
            )
            self.discovered_services.remove(service)
            self._logger.debug(
                "Received depart for service %s on host %s: Removed.",
                msg.serviceid,
                msg.from_address,
            )
            # indicate that service is no longer with us
            service.alive = False
            try:
                callback = self._chirp_callbacks[msg.serviceid]
                self._chirp_callbacks_q.put((callback, service))
            except KeyError:
                self._logger.debug("No callback for service %s set up.", msg.serviceid)
        except ValueError:
            self._logger.debug(
                "Received depart for service %s on host %s: Not in use.",
                msg.serviceid,
                msg.from_address,
            )

    def _run(self) -> None:
        """Start listening in on broadcast"""

        while not self._stop_broadcasting.is_set():
            msg = self._beacon.listen()
            if not msg:
                time.sleep(0.1)
                continue

            # Check Message Type
            if msg.msgtype == CHIRPMessageType.REQUEST:
                self.broadcast_offers(msg.serviceid)
                continue

            if msg.msgtype == CHIRPMessageType.OFFER:
                self._discover_service(msg)
                continue

            if msg.msgtype == CHIRPMessageType.DEPART and msg.port != 0:
                self._depart_service(msg)
                continue


def main(args=None):
    """Start a broadcast manager service."""
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument("--log-level", default="info")

    args = parser.parse_args(args)
    logging.basicConfig(
        format="%(asctime)s %(levelname)s %(name)s: %(message)s",
        level=args.log_level.upper(),
    )

    q = Queue()
    # start server with remaining args
    bcst = CHIRPBroadcastManager(
        name="broadcast_test",
        group="Constellation",
        callback_queue=q,
    )
    bcst.register_offer(CHIRPServiceIdentifier.HEARTBEAT, 50000)
    bcst.register_offer(CHIRPServiceIdentifier.CONTROL, 50001)
    print("Services registered")
    bcst.broadcast_offers()
    print("Broadcast for offers sent")
    bcst.start()
    print("Listener started, sleeping...")
    time.sleep(10)
    bcst.stop()


if __name__ == "__main__":
    main()
