import logging
import threading

import time
from uuid import UUID

from typing import Callable
from constellation.protocol import (
    CHIRPServiceIdentifier,
    CHIRPMessageType,
    CHIRPBeaconTransmitter,
)

# from .fsm import SatelliteState

""" Conecpt of broadcastmanager: services are not specified in the manner they handle the socket. For example: a service like HEARTBEAT offers other units the ability to
listen in on the hosts hb_port. Meanwhile a service like CONTROL offers other units the ability to send commands to the hosts cmd_port. It all depends on the callbacks hence why
DiscoverCallback is needed to specify action when receiving an OFFER. """


class RegisteredService:
    """Class to hold service description along with callback to service function."""

    def __init__(
        self,
        serviceid: CHIRPServiceIdentifier,
        port: int,
    ):
        """Initialize variables."""
        self.port = port
        self.serviceid = serviceid


class DiscoveredService:
    """Class to hold discovered service data."""

    def __init__(
        self,
        host_uuid: UUID,
        serviceid: CHIRPServiceIdentifier,
        address,
        port: int,
    ):
        """Initialize variables."""
        self.host_uuid = host_uuid
        self.address = address
        self.port = port
        self.serviceid = serviceid


class DiscoverCallback:
    """Class to hold callback to be used when discovering service."""

    def __init__(
        self,
        serviceid: CHIRPServiceIdentifier,
        callback: Callable,
    ):
        self.callback = callback
        self.serviceid = serviceid


class BroadcastManager:
    """Manages service discovery and broadcast via the CHIRP protocol."""

    def __init__(
        self,
        host_uuid: UUID,
        group_uuid: UUID,
    ):
        """Initialize parameters.

        :param host: name of the host the BroadcastManager represents
        :type host: string
        :param config: the configuration to be used.
        :type config:
        :param group:
        """
        self.host_uuid = host_uuid
        self._stop_broadcasting = threading.Event()
        self.group_uuid = group_uuid
        self.discovered_services = []
        self._beacon = CHIRPBeaconTransmitter(self.host_uuid, self.group_uuid)

        # Set up threads and depart events for callbacks
        self._callback_lock = threading.Lock()
        self._callback_threads = {}
        self._depart_events = {}

        # Register callbacks for services
        self._discover_callbacks = {}

        # Register offered services
        self._registered_services = []

    def start(self):
        """Start broadcast manager."""
        self._run_thread = threading.Thread(target=self.run, daemon=True)
        self._run_thread.start()

    def stop(self):
        self._stop_broadcasting.set()
        self._run_thread.join()

    def get_serviceid(self):
        """Returns a list of all serviceid from offered services."""
        return [service.serviceid for service in self._registered_services]

    def find_registered_callback(self, serviceid):
        """Find the registered callback if it exists."""
        for callback in self._discover_callbacks:
            if serviceid == callback.serviceid:
                return callback
        logging.info(f"Could not find callback for service with ID {serviceid}")

    def find_registered_service(self, serviceid):
        """Find the registered service if it exists."""
        for service in self._registered_services:
            if serviceid == service.serviceid:
                return service
        logging.info(f"Could not find service with ID {serviceid}")

    def find_discovered_service(
        self, host_uuid: UUID, serviceid: CHIRPServiceIdentifier, host_addr, port: int
    ):
        service = DiscoveredService(host_uuid, serviceid, host_addr, port)
        for discovered_service in self.discovered_services:
            if service == discovered_service:
                return discovered_service

    def broadcast_all(self):
        """Broadcast all registered services."""
        for service in self._registered_services:
            try:
                self._beacon.broadcast_service(
                    service.serviceid, CHIRPMessageType.OFFER, service.port
                )
                time.sleep(0.1)
            except RuntimeError:
                pass

    def register_callback(self, serviceid: CHIRPServiceIdentifier, callback: callable):
        """Register new callback for ServiceIdentifier."""
        if serviceid in self._discover_callbacks.keys():
            logging.info("Overwriting callback")
        self._discover_callbacks[serviceid] = callback

    def register_service(self, serviceid: CHIRPServiceIdentifier, port: int):
        """Register new offered service or overwrite existing service."""
        if serviceid in [service.serviceid for service in self._registered_services]:
            logging.info(f"Replacing registration for service ID {serviceid}")
        self._registered_services.append(RegisteredService(serviceid, port))
        self._beacon.broadcast_service(serviceid, CHIRPMessageType.OFFER, port)

    def discover_service(
        self, host_uuid: UUID, serviceid: CHIRPServiceIdentifier, host_addr, port: int
    ):
        """Add newly discovered service to internal list and return a DiscoveredService."""
        new_service = DiscoveredService(host_uuid, serviceid, host_addr, port)
        if new_service in self.discovered_services:
            logging.info("Service already discovered")
        else:
            self.discovered_services.append(new_service)
        return new_service

    def forget_service(
        self, host_uuid: UUID, serviceid: CHIRPServiceIdentifier, host_addr, port: int
    ):
        """Forget discovered service."""
        self.discovered_services.remove(
            DiscoveredService(host_uuid, serviceid, host_addr, port)
        )

    def request_service(self, serviceid):
        """Request service of type ServiceIdentifier."""
        if serviceid not in self._discover_callbacks.keys():
            logging.warn("Serviceid does not have a registered callback")
        self._beacon.broadcast_service(serviceid, CHIRPMessageType.REQUEST)

    def run(self):
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
                registered_service = self.find_registered_service(msg.serviceid)
                if registered_service:
                    self._beacon.broadcast_service(
                        registered_service._serviceid,
                        CHIRPMessageType.OFFER,
                        registered_service._port,
                    )
                    logging.info(
                        f"Broadcasting service {registered_service._serviceid}"
                    )
                continue

            if msg.msgtype == CHIRPMessageType.OFFER:
                discovered_service = self.discover_service(
                    msg.host_uuid, msg.serviceid, msg.from_address, msg.port
                )
                logging.info(
                    f"Received OFFER for service: {msg.serviceid} on host {msg.from_address}"
                )
                registered_callback = self.find_registered_callback(msg.serviceid)
                if registered_callback:
                    with self._callback_lock:
                        try:
                            depart_event = threading.Event()
                            self._callback_threads[
                                discovered_service
                            ] = threading.Thread(
                                target=registered_callback._callback,
                                args=(msg.from_address, msg.port, depart_event),
                                daemon=True,
                            )
                            self._callback_threads[discovered_service].start()
                            self._depart_events[discovered_service] = depart_event
                            logging.info("Found and started callback for service")
                        except Exception:
                            logging.error(
                                f"Could not issue callback for service: {msg.serviceid} on host with id: {msg.host_uuid}"
                            )
                continue

            if (
                msg.msgtype == CHIRPMessageType.DEPART
                and msg.port != 0
                and (
                    depart_service := self.find_discovered_service(
                        msg.host_uuid, msg.from_address, msg.port, msg.serviceid
                    )
                )
                in self.discovered_services
            ):
                self.forget_service(msg.host_uuid, msg.port)
                self._depart_events[depart_service].set()
                self._callback_threads[depart_service].join()
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
