"""
SPDX-FileCopyrightText: 2026 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2

This module provides an async CSCP command receiver.
"""

import asyncio
from typing import Any

import zmq
import zmq.asyncio
from statemachine.exceptions import TransitionNotAllowed

from constellation.core.base import BaseSatelliteFrame
from constellation.core.commandmanager import (
    _extract_payload_args,
    _get_signature,
    cscp_requestable,
    get_cscp_commands,
)
from constellation.core.message.cscp1 import CSCP1Message
from constellation.core.protocol.cscp1 import SatelliteState


class AsyncCSCPTransmitter:
    """Async CSCP transmitter using zmq.asyncio REP socket."""

    def __init__(self, name: str, socket: zmq.asyncio.Socket) -> None:
        self._name = name
        self._socket = socket

    async def get_message(self) -> CSCP1Message | None:
        """Await a single CSCP request from the socket.

        Uses asyncio polling with a short timeout so the caller can
        check the stop event periodically.
        """
        events = await self._socket.poll(timeout=500, flags=zmq.POLLIN)
        if not events:
            return None
        frames = await self._socket.recv_multipart()
        return CSCP1Message.disassemble(frames)

    async def send_reply(
        self,
        response: str,
        msgtype: CSCP1Message.Type,
        payload: Any = None,
        tags: dict[str, Any] | None = None,
    ) -> None:
        """Send a reply message on the async socket."""
        message = CSCP1Message(self._name, (msgtype, response), tags=tags)
        if payload is not None:
            message.payload = payload
        await self._socket.send_multipart(message.assemble().frames)

    def close(self) -> None:
        """Close the REP socket."""
        self._socket.close()


class AsyncCSCPReceiver(BaseSatelliteFrame):
    """Async CSCP command receiver.

    Binds a zmq.asyncio REP socket and processes CSCP requests as an
    asyncio coroutine. Reuses the cscp_requestable decorator and
    payload validation from commandmanager.
    """

    def __init__(self, cmd_port: int = 0, **kwds: Any) -> None:
        super().__init__(**kwds)

        self.log_cscp = self.get_logger("CTRL")

        sock = self._async_ctx.socket(zmq.REP)
        sock.setsockopt(zmq.LINGER, 2000)

        if not cmd_port:
            self.cmd_port = sock.bind_to_random_port("tcp://*")
        else:
            sock.bind(f"tcp://*:{cmd_port}")
            self.cmd_port = cmd_port

        self.log_cscp.info(f"Satellite listening on command port {self.cmd_port}")
        self._cmd_tm = AsyncCSCPTransmitter(self.name, sock)
        self._cmds = get_cscp_commands(self)

    def _add_com_task(self) -> None:
        """Register the CSCP receiver coroutine."""
        super()._add_com_task()
        self._com_task_factories.append(self._recv_cmds_async)

    async def _recv_cmds_async(self, stop: asyncio.Event) -> None:
        """CSCP request receive loop."""
        while not stop.is_set():
            try:
                req = await self._cmd_tm.get_message()
            except zmq.ZMQError as e:
                self.log_cscp.exception(e)
                await asyncio.sleep(0.5)
                continue
            if not req:
                continue

            if req.verb_type != CSCP1Message.Type.REQUEST:
                self.log_cscp.warning(f"Received malformed request with msg verb: {req.verb}")
                await self._cmd_tm.send_reply(
                    f"Received malformed request with msg verb: {req.verb}",
                    CSCP1Message.Type.INVALID,
                )
                continue

            command = req.verb_msg.lower()
            if command not in self._cmds:
                self.log_cscp.warning("Unknown command: %s", command)
                await self._cmd_tm.send_reply(f"Unknown command: {command}", CSCP1Message.Type.UNKNOWN)
                continue

            command_cb = getattr(self, command)
            if hasattr(self, "fsm") and hasattr(command_cb, "allowed_states"):
                state = self.fsm.state
                if command_cb.allowed_states is not None and state not in command_cb.allowed_states:
                    self.log_cscp.warning("Command not allowed in %s state: %s", state.name, req)
                    await self._cmd_tm.send_reply(f"Command not allowed in {state.name} state", CSCP1Message.Type.INVALID)
                    continue

            unpack_list = command_cb.unpack_list
            try:
                sig = _get_signature(command_cb)
                if sig:
                    self.log_cscp.debug("Calling command %s with payload %s", command, req.payload)
                    call_args = _extract_payload_args(req.payload, sig, unpack_list)
                    rv = command_cb(*call_args)
                else:
                    self.log_cscp.debug("Calling command %s with no arguments", command)
                    rv = command_cb()
                if rv is None:
                    self.log_cscp.warning("Command not allowed: %s", req)
                    await self._cmd_tm.send_reply("Command not allowed", CSCP1Message.Type.INVALID)
                    continue
                res, payload, tags = rv
            except (AttributeError, NotImplementedError) as e:
                self.log_cscp.error("Command failed with %s: %s", e, req)
                await self._cmd_tm.send_reply(
                    f"WrongImplementation: {e}",
                    CSCP1Message.Type.NOTIMPLEMENTED,
                    str(e),
                )
                continue
            except TransitionNotAllowed as e:
                self.log_cscp.warning("Transition `%s` not allowed: %s", command, e)
                await self._cmd_tm.send_reply(f"Transition not allowed: {e}", CSCP1Message.Type.INVALID, str(e))
                continue
            except (TypeError, ValueError) as e:
                self.log_cscp.error("Command `%s` received wrong argument: %s", command, str(e))
                await self._cmd_tm.send_reply(f"Wrong argument: {e}", CSCP1Message.Type.INCOMPLETE, str(e))
                continue
            except Exception as e:
                self.log_cscp.error("Command `%s` failed: %s", command, str(e))
                await self._cmd_tm.send_reply(f"Exception: {e}", CSCP1Message.Type.INVALID, str(e))
                continue

            if res is None:
                self.log_cscp.warning("Command `%s` returned nothing: %s", command, req)
                await self._cmd_tm.send_reply("Command returned nothing", CSCP1Message.Type.INCOMPLETE)
                continue

            self.log_cscp.debug("Command `%s` succeeded with `%s`: %s", command, res, req)
            try:
                await self._cmd_tm.send_reply(res, CSCP1Message.Type.SUCCESS, payload, tags)
            except TypeError as e:
                self.log_cscp.exception("Sending response `%s` failed: %s", res, e)
                await self._cmd_tm.send_reply(str(e), CSCP1Message.Type.ERROR, None)

        self.log_cscp.info("AsyncCSCPReceiver shutting down.")
        self._cmd_tm.close()

    def add_cscp_command(
        self,
        method: str,
        doc: str | None = None,
        allowed_states: list[SatelliteState] | None = None,
        unpack_list: bool = True,
    ) -> None:
        """Add a command to CSCP at runtime.

        Alternative to the @cscp_requestable() decorator.
        """
        from functools import wraps

        call = getattr(self, method)
        if not doc:
            doc = str(call.__doc__)

        @wraps(call)
        def wrapper(*args, **kwargs):
            return call(*args, **kwargs)

        if allowed_states is not None:
            setattr(wrapper, "allowed_states", allowed_states)  # noqa: B010
        setattr(wrapper, "unpack_list", unpack_list)  # noqa: B010

        setattr(self, method, wrapper)
        self._cmds[method] = doc

    @cscp_requestable()
    def get_commands(self) -> tuple[str, Any, dict[str, Any]]:
        """Return all commands supported by the Satellite."""
        from constellation.core.commandmanager import _format_signature

        public_cmds: dict[str, str] = {}
        for key in self._cmds:
            if not key.startswith("_"):
                cmd_func = getattr(self, key)
                sig = _get_signature(cmd_func)
                description = self._cmds[key] or ""
                if description:
                    description += "\n"
                description += _format_signature(sig)
                public_cmds[key] = description
        return f"{len(public_cmds)} commands known", public_cmds, {}

    @cscp_requestable()
    def _get_commands(self) -> tuple[str, Any, dict[str, Any]]:
        """Return all hidden commands supported by the Satellite."""
        from constellation.core.commandmanager import _format_signature

        hidden_cmds: dict[str, str] = {}
        for key in self._cmds:
            if key.startswith("_"):
                cmd_func = getattr(self, key)
                sig = _get_signature(cmd_func)
                description = self._cmds[key] or ""
                if description:
                    description += "\n"
                description += _format_signature(sig)
                hidden_cmds[key] = description
        return f"{len(hidden_cmds)} commands known", hidden_cmds, {}

    @cscp_requestable()
    def get_name(self) -> tuple[str, Any, dict[str, Any]]:
        """Return the canonical name of the Satellite."""
        return self.name, None, {}
