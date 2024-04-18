#!/usr/bin/env python3

"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0
"""

import logging
import threading
import time
from queue import Empty
from typing import Dict
from functools import partial

import zmq

from .broadcastmanager import CHIRPBroadcaster, chirp_callback, DiscoveredService
from .chirp import CHIRPServiceIdentifier, get_uuid

from .cscp import CommandTransmitter
from .error import debug_log
from .satellite import Satellite
from .commandmanager import get_cscp_commands


class SatelliteArray:
    """Provide object-oriented control of connected Satellites."""

    def __init__(self, group: str, handler: callable):
        self.constellation = group
        self._handler = handler
        # initialize with the commands known to any CSCP Satellite
        self._add_cmds(self, self._handler, get_cscp_commands(Satellite))
        self._satellites: list(SatelliteCommLink) = []

    @property
    def satellites(self):
        """Return the list of known Satellite."""
        return self._satellites

    def _add_class(self, name: str, commands: dict[str]):
        """Add a new class to the array."""
        try:
            cl = getattr(self, name)
            return cl
        except AttributeError:
            pass
        # add attributes now
        cl = SatelliteClassCommLink(name)
        self._add_cmds(cl, self._handler, commands)
        setattr(self, name, cl)
        return cl

    def _add_satellite(self, name: str, cls: str, commands: dict[str]):
        """Add a new Satellite."""
        try:
            cl = getattr(self, cls)
        except AttributeError:
            cl = self._add_class(cls, commands)
        sat = SatelliteCommLink(name, cls)
        self._add_cmds(sat, self._handler, commands)
        setattr(cl, name, sat)
        self._satellites.append(sat)
        return sat

    def _remove_satellite(self, uuid: str):
        """Remove a Satellite."""
        name, cls = self._get_name_from_uuid(uuid)
        # remove attribute
        delattr(getattr(self, cls), name)
        # clear from list
        self._satellites = [sat for sat in self._satellites if sat._uuid != uuid]

    def _get_name_from_uuid(self, uuid: str):
        s = [sat for sat in self._satellites if sat._uuid == uuid]
        if not s:
            raise KeyError("No Satellite with that UUID known.")
        name = s[0]._name
        cls = s[0]._class_name
        return name, cls

    def _add_cmds(self, obj: any, handler: callable, cmds: dict[str]):
        try:
            sat = obj._name
        except AttributeError:
            sat = None
        try:
            satcls = obj._class_name
        except AttributeError:
            satcls = None
        for cmd, doc in cmds.items():

            class wrapper:
                """Class to wrap partial calls w/ signature of orig. fcn."""

                def __init__(self, fcn):
                    """Initialize with fcn as a partial() call."""
                    self.fcn = fcn

                def call(self, payload=None):
                    """Perform call. This doc string will be overwritten."""
                    return self.fcn(payload)

            w = wrapper(partial(handler, sat=sat, satcls=satcls, cmd=cmd))
            # add docstring
            w.call.__func__.__doc__ = doc
            setattr(obj, cmd, w.call)


class SatelliteClassCommLink:
    """A link to a Satellite Class."""

    def __init__(self, name):
        self._class_name = name

    def __str__(self):
        """Convert to class name."""
        return self._class_name


class SatelliteCommLink(SatelliteClassCommLink):
    """A link to a Satellite."""

    def __init__(self, name, cls):
        self._name = name
        self._uuid = str(get_uuid(f"{cls}.{name}"))
        super().__init__(cls)

    def __str__(self):
        """Convert to canonical name."""
        return f"{self._class_name}.{self._name}"


class BaseController(CHIRPBroadcaster):
    """Simple controller class to send commands to a Constellation."""

    def __init__(self, name: str, group: str, interface: str):
        """Initialize values.

        Arguments:
        - name ::  name of controller
        - group ::  group of controller
        - interface :: the interface to connect to
        """
        super().__init__(name=name, group=group, interface=interface)

        self._transmitters: Dict[str, CommandTransmitter] = {}

        self.constellation = SatelliteArray(group, self.command)

        super()._add_com_thread()
        super()._start_com_threads()

        # set up thread to handle incoming tasks (e.g. CHIRP discoveries)
        self._task_handler_event = threading.Event()
        self._task_handler_thread = threading.Thread(
            target=self._run_task_handler, daemon=True
        )
        self._task_handler_thread.start()

        # wait for threads to be ready
        time.sleep(0.2)
        self.request(CHIRPServiceIdentifier.CONTROL)

    @debug_log
    @chirp_callback(CHIRPServiceIdentifier.CONTROL)
    def _add_satellite_callback(self, service: DiscoveredService):
        """Callback method connecting to satellite."""
        if not service.alive:
            self._remove_satellite(service)
        else:
            self._add_satellite(service)

    def _add_satellite(self, service: DiscoveredService):
        # create socket
        socket = self.context.socket(zmq.REQ)
        # configure send/recv timeouts to avoid hangs if Satellite fails
        socket.setsockopt(zmq.SNDTIMEO, 1000)
        socket.setsockopt(zmq.RCVTIMEO, 1000)
        socket.connect("tcp://" + service.address + ":" + str(service.port))
        ct = CommandTransmitter(self.name, socket)
        self.log.debug(
            "Connecting to %s, address %s on port %s...",
            service.host_uuid,
            service.address,
            service.port,
        )
        try:
            # get list of commands
            msg = ct.request_get_response("get_commands")
            # get canonical name
            cls, name = msg.from_host.split(".", maxsplit=1)
            sat = self.constellation._add_satellite(name, cls, msg.payload)
            if sat._uuid != str(service.host_uuid):
                self.log.warning(
                    "UUIDs do not match: expected %s but received %s",
                    sat._uuid,
                    str(service.host_uuid),
                )
            self._transmitters[str(service.host_uuid)] = ct
        except RuntimeError as e:
            self.log.error("Could not add Satellite %s: %s", service.host_uuid, repr(e))

    def _remove_satellite(self, service: DiscoveredService):
        name, cls = None, None
        # departure
        uuid = str(service.host_uuid)
        try:
            name, cls = self.constellation._get_name_from_uuid(uuid)
            self.constellation._remove_satellite(uuid)
        except KeyError:
            pass
        self.log.debug(
            "Departure of %s, known as %s.%s",
            service.host_uuid,
            name,
            cls,
        )
        try:
            ct = self._transmitters[uuid]
            ct.socket.close()
            self._transmitters.pop(uuid)
        except KeyError:
            pass

    def command(self, payload=None, sat=None, satcls=None, cmd=None):
        """Wrapper for _command_satellite function. Handle sending commands to all hosts"""
        targets = []
        # figure out whether to send command to Satellite, Class or whole Constellation
        if not sat and not satcls:
            targets = [sat._uuid for sat in self.constellation.satellites]
            self.log.info(
                "Sending %s to all %s connected Satellites.", cmd, len(targets)
            )
        elif not sat:
            targets = [
                sat._uuid
                for sat in self.constellation.satellites
                if sat._class_name == satcls
            ]
            self.log.info(
                "Sending %s to all %s connected Satellites of class %s.",
                cmd,
                len(targets),
                satcls,
            )
        else:
            targets = [getattr(getattr(self.constellation, satcls), sat)._uuid]
            self.log.info("Sending %s to Satellite %s.", cmd, targets[0])

        res = {}
        for target in targets:
            self.log.debug("Host %s send command %s...", target, cmd)

            try:
                ret_msg = self._transmitters[target].request_get_response(
                    command=cmd,
                    payload=payload,
                    meta=None,
                )
            except KeyError:
                self.log.error(
                    "Command %s failed for %s (%s.%s): No transmitter available",
                    cmd,
                    target,
                    satcls,
                    sat,
                )
                continue
            except RuntimeError as e:
                self.log.error(
                    "Command %s failed for %s (%s.%s): %s",
                    cmd,
                    target,
                    satcls,
                    sat,
                    repr(e),
                )
                continue
            self.log.debug(
                "%s responded: %s",
                ret_msg.from_host,
                ret_msg.msg,
            )
            if ret_msg.header_meta:
                self.log.debug("    header: %s", ret_msg.header_meta)
            if ret_msg.payload:
                self.log.debug("    payload: %s", ret_msg.payload)
            if sat:
                # simplify return value for single satellite
                res = {"msg": ret_msg.msg, "payload": ret_msg.payload}
            else:
                # append
                res[ret_msg.from_host] = {
                    "msg": ret_msg.msg,
                    "payload": ret_msg.payload,
                }
        return res

    def _run_task_handler(self):
        """Event loop for task handler-routine"""
        while not self._task_handler_event.is_set():
            try:
                # blocking call but with timeout to prevent deadlocks
                task = self.task_queue.get(block=True, timeout=0.5)
                callback = task[0]
                args = task[1]
                try:
                    callback(*args)
                except Exception as e:
                    self.log.exception(e)
            except Empty:
                # nothing to process
                pass

    def reentry(self):
        """Stop the controller."""
        self.log.info("Stopping controller.")
        if getattr(self, "_task_handler_event", None):
            self._task_handler_event.set()
        for _name, cmd_tm in self._transmitters.items():
            cmd_tm.socket.close()
        if getattr(self, "_task_handler_event", None):
            self._task_handler_thread.join()
        super().reentry()


def main():
    """Start a controller."""
    import argparse
    import coloredlogs
    from IPython import embed

    parser = argparse.ArgumentParser()
    parser.add_argument("--log-level", default="info")
    parser.add_argument("--name", type=str, default="cli_controller")
    parser.add_argument("--group", type=str, default="constellation")
    parser.add_argument("--interface", type=str, default="*")

    args = parser.parse_args()

    # set up logging
    logger = logging.getLogger(args.name)
    coloredlogs.install(level=args.log_level.upper(), logger=logger)

    logger.debug("Starting up CLI Controller!")

    # start server with args
    ctrl = BaseController(name=args.name, group=args.group, interface=args.interface)

    constellation = ctrl.constellation  # noqa

    print("\nWelcome to the Constellation CLI IPython Controller!\n")
    print(
        "You can interact with the discovered Satellites via the `constellation` array:"
    )
    print("          constellation.get_state()\n")
    print("To get help for any of its methods, call it with a question mark:")
    print("          constellation.get_state?\n")
    print("Happy hacking! :)\n")

    # start IPython console
    embed()


if __name__ == "__main__":
    main()