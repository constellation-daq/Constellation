#!/usr/bin/env python3

"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0
"""

import logging
import threading
from queue import Empty
from typing import Dict

import zmq
from functools import partial
from IPython import embed

from .broadcastmanager import CHIRPBroadcaster, chirp_callback, DiscoveredService
from .chirp import CHIRPServiceIdentifier, get_uuid

# from .confighandler import get_config
from .cscp import CommandTransmitter
from .error import debug_log

from .commandmanager import COMMANDS
from .satellite import Satellite  # noqa


class SatelliteArray:
    def __init__(self, group: str, handler: callable):
        self.constellation = group
        self._handler = handler
        # initialize with the commands known to any CSCP Satellite
        self._add_cmds(self, self._handler, COMMANDS)
        self._satellites: list(SatelliteCommLink) = []

    @property
    def satellites(self):
        return self._satellites

    def _add_class(self, name, commands):
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

    def _add_satellite(self, name, cls, commands):
        try:
            cl = getattr(self, cls)
        except AttributeError:
            cl = self._add_class(cls, commands)
        sat = SatelliteCommLink(name, cls)
        self._add_cmds(sat, self._handler, commands)
        setattr(cl, name, sat)
        self._satellites.add(sat)
        return sat

    def _remove_satellite(self, name, cls):
        # remove attribute
        delattr(self, f"{cls}.{name}")
        # clear from list
        self._satellites = [
            sat for sat in self._satellites if sat.name != name or sat.class_name != cls
        ]

    def _add_cmds(self, obj, handler, cmds):
        try:
            sat = obj.name
        except AttributeError:
            sat = None
        try:
            satcls = obj.class_name
        except AttributeError:
            satcls = None
        for cmd in cmds:
            setattr(obj, cmd, partial(handler, sat=sat, satcls=satcls, cmd=cmd))


class SatelliteClassCommLink:
    def __init__(self, name):
        self.class_name = name


class SatelliteCommLink(SatelliteClassCommLink):
    def __init__(self, name, cls):
        self.name = name
        self.uuid = get_uuid(f"{cls}.{name}")
        super().__init__(cls)


class BaseController(CHIRPBroadcaster):
    """Simple controller class to send commands to a list of satellites."""

    def __init__(self, name: str, group: str, hosts=None):
        """Initialize values.

        Arguments:
        - name ::  name of controller
        - group ::  group of controller
        - hosts ::  name, address and port of satellites to control
        """
        super().__init__(name=name, group=group)

        self.transmitters: Dict[str, CommandTransmitter] = {}

        self.constellation = SatelliteArray(group, self.command)

        super()._add_com_thread()
        super()._start_com_threads()

        if hosts:
            for host in hosts:
                self._add_satellite(host_name=host, host_addr=host)

        self.request(CHIRPServiceIdentifier.CONTROL)
        self._task_handler_event = threading.Event()
        self.task_handler_thread = threading.Thread(
            target=self._run_task_handler, daemon=True
        )
        self.task_handler_thread.start()

    @debug_log
    @chirp_callback(CHIRPServiceIdentifier.CONTROL)
    def _add_satellite_callback(self, service: DiscoveredService):
        """Callback method connecting to satellite."""
        # TODO handle departures
        # configure send/recv timeouts to avoid hangs if Satellite fails
        self.context.setOption(zmq.ZMQ_RCVTIMEO, 1000)
        self.context.setOption(zmq.ZMQ_SNDTIMEO, 1000)
        # create socket
        socket = self.context.socket(zmq.REQ)
        socket.connect("tcp://" + service.address + ":" + str(service.port))
        ct = CommandTransmitter(service.host_uuid, socket)
        self.log.info(
            "connecting to %s, address %s on port %s...",
            service.host_uuid,
            service.address,
            service.port,
        )
        try:
            # get canonical name
            msg = ct.request_get_response("get_name")
            cls, name = msg.msg.split(".", maxsplit=1)
            # get list of commands
            msg = ct.request_get_response("get_commands")
            cmds = msg.payload
            self.constellation._add_satellite(name, cls, cmds)
            self.transmitters[str(service.host_uuid)] = ct
        except RuntimeError as e:
            self.log.error("Could not add Satellite %s: %s", service.host_uuid, repr(e))

    def command(self, payload=None, sat=None, satcls=None, cmd=None):
        """Wrapper for _command_satellite function. Handle sending commands to all hosts"""

        targets = []
        # figure out whether to send command to Satellite, Class or whole Constellation
        if not sat and not satcls:
            targets = [sat.uuid for sat in self.constellation]
        elif not sat:
            targets = [
                sat.uuid for sat in self.constellation if sat.class_name == satcls
            ]
        else:
            targets = [getattr(getattr(self.constellation, satcls), sat).uuid]

        for target in targets:
            self.log.info("Host %s send command %s...", target, cmd)

            try:
                ret_msg = self.transmitters[target].request_get_response(
                    command=cmd,
                    payload=payload,
                    meta=None,
                )
            except KeyError:
                self.log.error(
                    "Command %s failed for %s.%s: No transmitter available",
                    cmd,
                    satcls,
                    sat,
                )
                continue
            self.log.info(
                "Host %s sent response: %s, %s",
                target,
                ret_msg.msg_verb,
                ret_msg.msg,
            )
            if ret_msg.header_meta:
                self.log.info("    header: %s", ret_msg.header_meta)
            if ret_msg.payload:
                self.log.info("    payload: %s", ret_msg.payload)

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
        self._task_handler_event.set()
        for _name, cmd_tm in self.transmitters.items():
            cmd_tm.socket.close()
        self.task_handler_thread.join()


class SatelliteManager(BaseController):
    """Satellite Manager class implementing CHIRP protocol"""

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)


def main():
    """Start a controller."""
    import argparse
    import coloredlogs

    parser = argparse.ArgumentParser()
    parser.add_argument("--log-level", default="info")
    parser.add_argument("--name", type=str, default="controller_demo")
    parser.add_argument("--group", type=str, default="constellation")

    args = parser.parse_args()

    # set up logging
    logger = logging.getLogger(args.name)
    coloredlogs.install(level=args.log_level.upper(), logger=logger)

    logger.debug("Starting up CLI Controller!")

    # start server with args
    ctrl = BaseController(name=args.name, group=args.group)  # noqa

    print("\nWelcome to the Constellation CLI Controller!\n")
    print(
        "You can interact with the discovered Satellites via the `ctrl.constellation` array.\n"
    )
    print("Happy hacking! :)\n")

    # start IPython console
    embed()


if __name__ == "__main__":
    main()
