import logging
import threading

import time
from uuid import UUID
from queue import Queue

from constellation.protocol import (
    CHIRPServiceIdentifier,
    CHIRPMessage,
    CHIRPMessageType,
    CHIRPBeaconTransmitter,
)

""" Conecpt of broadcastmanager: services are not specified in the manner they handle the socket. For example: a service like HEARTBEAT offers other units the ability to
listen in on the hosts hb_port. Meanwhile a service like CONTROL offers other units the ability to send commands to the hosts cmd_port. It all depends on the callbacks hence why
DiscoverCallback is needed to specify action when receiving an OFFER. """


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
        """Comparison operator for network-related properites."""
        return self.address == other.address and self.port == other.port


class BroadcastManager:
    """Manages service discovery and broadcast via the CHIRP protocol."""

    def __init__(
        self,
        host_uuid: UUID,
        group_uuid: UUID,
        callback_queue: Queue,
    ):
        """Initialize parameters.

        :param host: UUID of the Satellite name the BroadcastManager represents
        :type host: UUID
        :param group: UUID of the group the Satellite belongs to
        :type group: UUID
        :param callback_queue: The queue in which discovery callbacks will be placed.
        :type callback_queue: Queue
        """
        self.host_uuid = host_uuid
        self.group_uuid = group_uuid
        self._stop_broadcasting = threading.Event()
        self._beacon = CHIRPBeaconTransmitter(self.host_uuid, self.group_uuid)

        # Register callbacks for services
        self._callbacks = {}
        self._callback_queue = callback_queue

        # Offered and discovered services
        self._registered_services = {}
        self.discovered_services = []

    def start(self) -> None:
        """Start broadcast manager."""
        self._run_thread = threading.Thread(target=self._run, daemon=True)
        self._run_thread.start()

    def stop(self) -> None:
        """Indicate broadcast manager to stop."""
        self.broadcast_depart()
        self._stop_broadcasting.set()
        self._run_thread.join()

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
        if serviceid in self._callbacks.keys():
            logging.info("Overwriting callback")
        self._callbacks[serviceid] = callback

    def register_offer(self, serviceid: CHIRPServiceIdentifier, port: int) -> None:
        """Register new offered service or overwrite existing service."""
        if port in self._registered_services.keys():
            logging.warning(f"Replacing service registration for port {port} from ")
        self._registered_services[port] = serviceid

    def request(self, serviceid: CHIRPServiceIdentifier) -> None:
        """Request specific service.

        Should already have a registered callback via register_request(), or
        any incoming OFFERS will go unnoticed.

        """
        if serviceid not in self._callbacks.keys():
            logging.warn(f"Serviceid {serviceid} does not have a registered callback")
        self._beacon.broadcast(serviceid, CHIRPMessageType.REQUEST)

    def broadcast_offers(self, serviceid: CHIRPServiceIdentifier = None) -> None:
        """Broadcast all registered services matching serviceid.

        Specify None for all registered services.

        """
        for port, sid in self._registered_services.items():
            if not serviceid or serviceid == sid:
                logging.debug(f"Broadcasting service OFFER on {port} for {sid}")
                self._beacon.broadcast(sid, CHIRPMessageType.OFFER, port)

    def broadcast_requests(self) -> None:
        """Broadcast all requests registered via register_request()."""
        for serviceid in self._callbacks.keys():
            logging.debug(f"Broadcasting service REQUEST for {serviceid}")
            self._beacon.broadcast(serviceid, CHIRPMessageType.REQUEST)

    def broadcast_depart(self) -> None:
        """Broadcast DEPART for all registered services."""
        for port, sid in self._registered_services.items():
            logging.debug(f"Broadcasting service DEPART on {port} for {sid}")
            self._beacon.broadcast(sid, CHIRPMessageType.DEPART, port)

    def _discover_service(self, msg: CHIRPMessage) -> None:
        """Add a service to internal list and possibly queue a callback."""
        service = DiscoveredService(
            msg.host_uuid, msg.serviceid, msg.from_address, msg.port
        )
        if service in self.discovered_services:
            logging.info(
                f"Service already discovered: {msg.serviceid} on host {msg.from_address}"
            )
        else:
            # add service to internal list and queue callback (if registered)
            logging.info(
                f"Received new OFFER for service: {msg.serviceid} on host {msg.from_address}"
            )
            try:
                callback = self._callbacks[msg.serviceid]
                self._callback_queue.put((callback, service))
            except KeyError:
                logging.debug(f"No callback for service {msg.serviceid} set up.")
            self.discovered_services.append(service)

    def _depart_service(self, msg: CHIRPMessage) -> None:
        """Depart with a service."""
        try:
            service = DiscoveredService(
                msg.host_uuid, msg.serviceid, msg.from_addr, msg.port
            )
            self.discovered_services.remove(service)
            logging.debug(
                f"Received depart for service {msg.serviceid} on host {msg.from_address}: Removed."
            )
            # indicate that service is no longer with us
            service.alive = False
            try:
                callback = self._callbacks[msg.serviceid]
                self._callback_queue.put((callback, service))
            except KeyError:
                logging.debug(f"No callback for service {msg.serviceid} set up.")
        except ValueError:
            logging.debug(
                f"Received depart for service {msg.serviceid} on host {msg.from_address}: Not in use."
            )

    def _run(self) -> None:
        """Start listening in on broadcast"""

        while not self._stop_broadcasting.is_set():
            msg = self._beacon.listen()
            if not msg:
                time.sleep(0.1)
                continue

            if msg.group_uuid != self.group_uuid:
                continue

            # Check Message Type
            if msg.msgtype == CHIRPMessageType.REQUEST:
                self.broadcast_offers(msg.serviceid)
                continue

            if msg.msgtype == CHIRPMessageType.OFFER:
                self._discover_service(msg)
                continue

            if msg.msgtype == CHIRPMessageType.DEPART and msg.port != 0:
                self._depart_service(msg.host_uuid, msg.port)
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

    import uuid

    # start server with remaining args
    s = BroadcastManager(
        host_uuid=uuid.uuid4(),
        group_uuid=uuid.uuid4(),
    )

    s.start()
    s.register_service(CHIRPServiceIdentifier.HEARTBEAT, 50000)
    s.register_service(CHIRPServiceIdentifier.CONTROL, 50001)
    print("Services registered, sleeping...")
    time.sleep(10)
    s.stop()


if __name__ == "__main__":
    main()
