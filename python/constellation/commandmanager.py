#!/usr/bin/env python3
"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0

This module provides classes for managing CSCP requests/replies within
Constellation Satellites.
"""

import logging
import threading
import time
from .cscp import CommandTransmitter, CSCPMessageVerb


COMMANDS = dict()


def cscp_requestable(func):
    """Register a function as a supported command for CSCP."""
    COMMANDS[func.__name__] = func
    return func


class BaseCommandReceiver:
    """Base class for handling incoming CSCP requests.

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

    def __init__(self, name, socket):
        self._cmd_tm = CommandTransmitter(name, socket)
        self._logger = logging.getLogger(name)
        self._stop_cmd_recv = threading.Event()
        self._cmd_thread = threading.Thread(target=self._recv_cmds, daemon=True)
        self._cmd_thread.start()

    def _recv_cmds(self):
        """Request receive loop."""
        while not self._stop_cmd_recv.is_set():
            req = self._cmd_tm.get_message()
            if not req:
                time.sleep(0.1)
                continue
            # check that it is actually a REQUEST
            if req.msg_verb != CSCPMessageVerb.REQUEST:
                self._logger.error(
                    f"Received malformed request with msg verb: {req.msg_verb}"
                )
                self._cmd_tm.send_reply("Unknown command", CSCPMessageVerb.INVALID)
                continue

            # find a matching callback
            try:
                callback = COMMANDS[req.msg]
            except KeyError:
                self._logger.error("Unknown command: %s", req)
                self._cmd_tm.send_reply("Unknown command", CSCPMessageVerb.UNKNOWN)
                continue
            # test whether callback is allowed by calling the
            # method "_COMMAND_is_allowed" (if exists).
            try:
                is_allowed = getattr(self, f"_{req.msg}_is_allowed")(req)
                if not is_allowed:
                    self._logger.error("Command not allowed: %s", req)
                    self._cmd_tm.send_reply("Not allowed", CSCPMessageVerb.INVALID)
                    continue
            except AttributeError:
                pass
            # perform the actual callback
            try:
                res, payload, meta = callback(self, req)
            except AttributeError as e:
                self._logger.error("Command failed with %s: %s", e, req)
                self._cmd_tm.send_reply(
                    "AttributeError", CSCPMessageVerb.NOTIMPLEMENTED
                )
                continue
            except Exception as e:
                self._logger.error("Command not allowed: %s", req)
                self._cmd_tm.send_reply(f"Exception {repr(e)}", CSCPMessageVerb.INVALID)
                continue
            # check the response; empty string means 'missing data/incomplete'
            if not res:
                self._logger.error("Command returned nothing: %s", req)
                self._cmd_tm.send_reply(
                    "Command returned nothing", CSCPMessageVerb.INCOMPLETE
                )
                continue
            # finally, assemble a proper response!
            self._logger.debug("Command succeeded with '%s': %s", res, req)
            self._cmd_tm.send_reply(res, CSCPMessageVerb.SUCCESS, payload, meta)
