#!/usr/bin/env python3
"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0

This module provides classes for managing CSCP requests/replies within
Constellation Satellites.
"""

import threading
import time
import zmq
from statemachine.exceptions import TransitionNotAllowed

from .cscp import CommandTransmitter, CSCPMessageVerb
from .base import BaseSatelliteFrame


COMMANDS = dict()


def cscp_requestable(func):
    """Register a function as a supported command for CSCP."""
    COMMANDS[func.__name__] = func
    return func


class CommandReceiver(BaseSatelliteFrame):
    """Class for handling incoming CSCP requests.

    Commands will call specific methods of the inheriting class which should
    have the following signature:

    def COMMAND(self, request: cscp.CSCPMessage) -> (str, any, dict):

    Inheriting classes need to decorate such command methods with
    '@cscp_requestable' to make them callable through CSCP requests.

    If a method

    def _COMMAND_is_allowed(self, request: cscp.CSCPMessage) -> bool:

    exists, it will be called first to determine whether the command is
    currently allowed or not.

    """

    def __init__(self, name: str, cmd_port: int, **kwds):
        """Initialize the Receiver and set up a ZMQ REP socket on given port."""
        super().__init__(name, **kwds)

        # set up the command channel
        sock = self.context.socket(zmq.REP)
        sock.bind(f"tcp://*:{cmd_port}")
        self.log.info(f"Satellite listening on command port {cmd_port}")
        self._cmd_tm = CommandTransmitter(name, sock)

    def _add_com_thread(self):
        """Add the command receiver thread to the communication thread pool."""
        super()._add_com_thread()
        self._com_thread_pool["cmd_receiver"] = threading.Thread(
            target=self._recv_cmds, daemon=True
        )
        self.log.debug("Command receiver thread prepared and added to the pool.")

    def _recv_cmds(self):
        """Request receive loop."""
        while not self._com_thread_evt.is_set():
            req = self._cmd_tm.get_message()
            if not req:
                time.sleep(0.01)
                continue
            # check that it is actually a REQUEST
            if req.msg_verb != CSCPMessageVerb.REQUEST:
                self.log.error(
                    f"Received malformed request with msg verb: {req.msg_verb}"
                )
                self._cmd_tm.send_reply("Unknown command", CSCPMessageVerb.INVALID)
                continue

            # find a matching callback
            try:
                callback = COMMANDS[req.msg]
            except KeyError:
                self.log.error("Unknown command: %s", req)
                self._cmd_tm.send_reply("Unknown command", CSCPMessageVerb.UNKNOWN)
                continue
            # test whether callback is allowed by calling the
            # method "_COMMAND_is_allowed" (if exists).
            try:
                is_allowed = getattr(self, f"_{req.msg}_is_allowed")(req)
                if not is_allowed:
                    self.log.error("Command not allowed: %s", req)
                    self._cmd_tm.send_reply("Not allowed", CSCPMessageVerb.INVALID)
                    continue
            except AttributeError:
                pass
            # perform the actual callback
            try:
                self.log.debug("Calling command %s with argument %s", callback, req)
                res, payload, meta = callback(self, req)
            except (AttributeError, ValueError, TypeError, NotImplementedError) as e:
                self.log.error("Command failed with %s: %s", e, req)
                self._cmd_tm.send_reply(
                    "WrongImplementation", CSCPMessageVerb.NOTIMPLEMENTED, repr(e)
                )
                continue
            except TransitionNotAllowed:
                self.log.error("Transition %s not allowed", req.msg)
                self._cmd_tm.send_reply(
                    "Transition not allowed", CSCPMessageVerb.INVALID, None
                )
                continue
            except Exception as e:
                self.log.error("Command not allowed: %s", req)
                self._cmd_tm.send_reply("Exception", CSCPMessageVerb.INVALID, repr(e))
                continue
            # check the response; empty string means 'missing data/incomplete'
            if not res:
                self.log.error("Command returned nothing: %s", req)
                self._cmd_tm.send_reply(
                    "Command returned nothing", CSCPMessageVerb.INCOMPLETE
                )
                continue
            # finally, assemble a proper response!
            self.log.debug("Command succeeded with '%s': %s", res, req)
            self._cmd_tm.send_reply(res, CSCPMessageVerb.SUCCESS, payload, meta)
        self.log.info("CommandReceiver thread shutting down.")
        # shutdown
        self._cmd_tm.socket.close()
