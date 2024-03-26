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

from .cscp import CommandTransmitter, CSCPMessageVerb, CSCPMessage
from .base import BaseSatelliteFrame


# store command names of decorated methods
COMMANDS = []


def cscp_requestable(func):
    """Register a function as a supported command for CSCP.

    See CommandReceiver for a description of the expected signature.

    """
    COMMANDS.append(func.__name__)
    return func


class CommandReceiver(BaseSatelliteFrame):
    """Class for handling incoming CSCP requests.

    Commands will call specific methods of the inheriting class which should
    have the following signature:

    def COMMAND(self, request: cscp.CSCPMessage) -> (str, any, dict):

    The expected return values are:
    - reply message (string)
    - payload (any)
    - map (dictionary) (e.g. for meta information)

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
        self._cmd_tm = CommandTransmitter(self.name, sock)
        # cached list of supported commands
        self._cmds = []

    def _add_com_thread(self):
        """Add the command receiver thread to the communication thread pool."""
        super()._add_com_thread()
        self._com_thread_pool["cmd_receiver"] = threading.Thread(
            target=self._recv_cmds, daemon=True
        )
        self.log.debug("Command receiver thread prepared and added to the pool.")

    def _recv_cmds(self):
        """Request receive loop."""
        # first, determine the supported commands
        #
        # NOTE the global list COMMANDS might include methods unavailble in this
        # class if e.g. different classes inheriting from Satellite were imported.
        # This step reduces the list to what is actually available.
        self._cmds = [cmd for cmd in COMMANDS if hasattr(self, cmd)]
        while not self._com_thread_evt.is_set():
            try:
                req = self._cmd_tm.get_message(flags=zmq.NOBLOCK)
            except zmq.ZMQError as e:
                # something wrong with the ZMQ socket, wait a while for recovery
                self.log.exception(e)
                time.sleep(0.5)
                continue
            if not req:
                # no message waiting for us, rest until next attempt
                time.sleep(0.025)
                continue
            # check that it is actually a REQUEST
            if req.msg_verb != CSCPMessageVerb.REQUEST:
                self.log.error(
                    f"Received malformed request with msg verb: {req.msg_verb}"
                )
                self._cmd_tm.send_reply("Unknown command", CSCPMessageVerb.INVALID)
                continue

            # find a matching callback
            if req.msg not in self._cmds:
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
                self.log.debug("Calling command %s with argument %s", req.msg, req)
                res, payload, meta = getattr(self, req.msg)(req)
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
            try:
                self._cmd_tm.send_reply(res, CSCPMessageVerb.SUCCESS, payload, meta)
            except TypeError as e:
                self.log.exception("Sending response '%s' failed: %s", res, e)
                self._cmd_tm.send_reply(str(e), CSCPMessageVerb.ERROR, None, None)
        self.log.info("CommandReceiver thread shutting down.")
        # shutdown
        self._cmd_tm.socket.close()

    @cscp_requestable
    def get_commands(self, _request: CSCPMessage = None):
        """Return all commands supported by the Satellite.

        No payload argument.

        This will include all methods with the @cscp_requestable decorator. The
        doc string of the function will be used to derive the summary and
        payload argument description for each command by using the first and the
        second line of the doc string, respectively (not counting empty lines).

        """
        res = []
        for cmd in self._cmds:
            summary = "missing docstring"
            payload_desc = "no payload/missing docstring"
            try:
                fcn = getattr(self, cmd)
                doc = [line for line in fcn.__doc__.splitlines() if line]
                summary = doc[0].strip()
                payload_desc = doc[1].strip()
            except (IndexError, AttributeError):
                pass
            res.append([cmd, summary, payload_desc])
        return f"{len(res)} commands known", res, None

    @cscp_requestable
    def get_class(self, _request: CSCPMessage = None):
        """Return the class of the Satellite.

        No payload argument.

        """
        return type(self).__name__, None, None
