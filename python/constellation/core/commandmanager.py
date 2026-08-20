"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2

This module provides classes for managing CSCP requests/replies within
Constellation Satellites.
"""

import inspect
import threading
import time
from collections.abc import Callable
from functools import wraps
from typing import Any, ParamSpec, TypeVar

import zmq
from statemachine.exceptions import TransitionNotAllowed

from .base import BaseSatelliteFrame
from .cscp import CommandTransmitter
from .message.cscp1 import CSCP1Message
from .protocol.cscp1 import SatelliteState

T = TypeVar("T")
P = ParamSpec("P")


def cscp_requestable(
    allowed_states: list[SatelliteState] | None = None,
    unpack_list: bool = True,
) -> Callable[[Callable[P, T]], Callable[P, T]]:
    """Register a function as a supported command for CSCP.

    See CommandReceiver for a description of the expected signature.

    """

    def decorator(func: Callable[P, T]) -> Callable[P, T]:
        @wraps(func)
        def wrapper(*args: P.args, **kwargs: P.kwargs) -> T:
            return func(*args, **kwargs)

        # mark function as cscp command
        setattr(wrapper, "cscp_command", True)  # noqa: B010
        setattr(wrapper, "unpack_list", unpack_list)  # noqa: B010
        if allowed_states is not None:
            setattr(wrapper, "allowed_states", allowed_states)  # noqa: B010

        return wrapper

    return decorator


def get_cscp_commands(cls: Any) -> dict[str, str]:
    """Loop over all class methods and return those marked as CSCP commands."""
    res = {}
    for func in dir(cls):
        if isinstance(getattr(type(cls), func, None), property):
            # skip properties
            continue
        call = getattr(cls, func)
        if callable(call) and not func.startswith("__"):
            # regular method
            if hasattr(call, "cscp_command") and getattr(call, "cscp_command"):  # noqa: B009
                doc = call.__doc__
                res[func] = doc
    return res


def _get_signature(func: Callable[..., Any]) -> list[tuple[str, str]]:
    """Extract parameter names and type annotations from a command function.

    Returns a list of ``(parameter_name, annotation_string)`` tuples for each parameter after ``self``.
    Simple annotations (``bool``, ``int``,  ``float``, ``str``) are cleaned up to their short form.
    Complex typing objects are forwarded as-is via ``str()``.
    """
    # Unwrap through decorators to get the original function signature.
    # Bound methods lose the __wrapped__ information that @functools.wraps
    # sets, so we need to extract the underlying function first.
    unwrapped = inspect.unwrap(func)
    # For bound methods, unwrap returns the underlying function, but when
    # inspecting an unbound function we need to skip 'self'.
    signature = inspect.signature(unwrapped)
    parameters: list[tuple[str, str]] = []
    # Check if the first parameter looks like 'self'
    param_iter = iter(signature.parameters.values())
    first_param = next(param_iter, None)
    if first_param is not None and first_param.name == "self":
        # We're inspecting an unbound function, skip self
        pass
    else:
        # Re-include the first param since it's not self
        param_iter = iter(signature.parameters.values())

    for parameter in param_iter:
        if parameter.annotation is inspect.Parameter.empty:
            parameters.append((parameter.name, ""))
        else:
            ann = str(parameter.annotation)
            # Strip the <class '...'> wrapper for simple built-in types
            if ann.startswith("<class '") and ann.endswith("'>"):
                ann = ann[len("<class '") : -len("'>")]
            parameters.append((parameter.name, ann))
    return parameters


def _format_signature(parameters: list[tuple[str, str]]) -> str:
    """Format a signature list into a human-readable string."""
    if not parameters:
        return "No arguments."
    parts = ", ".join(f"{name}: {ann}" if ann else name for name, ann in parameters)
    return f"Arguments: {parts}."


def _validate_payload_type(name: str, annotation: str, value: Any) -> None:
    """Validate a payload value against a simple type annotation.

    Checks ``bool``, ``str``, ``int``, and ``float``. More complex
    annotations (unions, generics, ``dict``, etc.) are ignored.
    """
    if annotation == "bool":
        if not isinstance(value, bool):
            raise TypeError(f"Parameter '{name}' must be bool, got {type(value).__name__}")
    elif annotation == "int":
        if not isinstance(value, int):
            raise TypeError(f"Parameter '{name}' must be int, got {type(value).__name__}")
    elif annotation == "float":
        if not isinstance(value, (int, float)):
            raise TypeError(f"Parameter '{name}' must be float, got {type(value).__name__}")
    elif annotation == "str":
        if not isinstance(value, str):
            raise TypeError(f"Parameter '{name}' must be str, got {type(value).__name__}")


def _extract_payload_args(payload: Any, sig: list[tuple[str, str]], unpack_list: bool) -> list[Any]:
    """Extract positional arguments from the CSCP payload based on the signature."""
    if not unpack_list:
        _validate_payload_type(*sig[0], payload)
        return [payload]
    if not isinstance(payload, list):
        raise TypeError(f"Payload must be a list with {len(sig)} elements, got {type(payload).__name__}")
    if len(payload) != len(sig):
        raise TypeError(f"Expected {len(sig)} payload elements, got {len(payload)}")
    # Validate each argument
    for (name, annotation), value in zip(sig, payload, strict=True):
        _validate_payload_type(name, annotation, value)
    return payload


class CommandReceiver(BaseSatelliteFrame):
    """Class for handling incoming CSCP requests.

    Commands will call specific methods of the inheriting class which should
    have the following signature:

        def COMMAND(self, ...) -> tuple[str, Any, dict[str, Any]]:

    The expected return values are:
    - reply message (string)
    - payload (any)
    - map (dictionary) (e.g. for meta information)

    Inheriting classes need to decorate such command methods with
    `@cscp_requestable()` to make them callable through CSCP requests.

    The payload of the incoming CSCP request is automatically unpacked and
    forwarded as individual positional arguments to the command method.
    Command methods should declare their expected payload arguments as
    positional parameters after ``self``.
    """

    def __init__(self, name: str, cmd_port: int, **kwds: Any):
        """Initialize the Receiver and set up a ZMQ REP socket on given port."""
        super().__init__(name=name, **kwds)

        self.log_cscp = self.get_logger("CTRL")

        # set up the command channel
        sock = self.context.socket(zmq.REP)
        # Set linger period for socket shutdown to avoid long hangs shutting
        # down [ms]
        sock.setsockopt(zmq.LINGER, 2000)
        # Set maximum time before a recv operation returns with EAGAIN [ms]
        sock.setsockopt(zmq.RCVTIMEO, 5000)
        if not cmd_port:
            self.cmd_port = sock.bind_to_random_port("tcp://*")
        else:
            sock.bind(f"tcp://*:{cmd_port}")
            self.cmd_port = cmd_port

        self.log_cscp.info(f"Satellite listening on command port {self.cmd_port}")
        self._cmd_tm = CommandTransmitter(self.name, sock)
        # cached list of supported commands
        self._cmds = get_cscp_commands(self)

    def _add_com_thread(self) -> None:
        """Add the command receiver thread to the communication thread pool."""
        super()._add_com_thread()
        self._com_thread_pool["cmd_receiver"] = threading.Thread(target=self._recv_cmds, daemon=True)
        self.log_cscp.debug("Command receiver thread prepared and added to the pool.")

    def _recv_cmds(self) -> None:
        """Request receive loop."""
        # assert for mypy static type analysis
        assert isinstance(self._com_thread_evt, threading.Event), "Thread Event not set up correctly"
        while not self._com_thread_evt.is_set():
            try:
                req = self._cmd_tm.get_message(flags=zmq.NOBLOCK)
            except zmq.ZMQError as e:
                # something wrong with the ZMQ socket, wait a while for recovery
                self.log_cscp.exception(e)
                time.sleep(0.5)
                continue
            if not req:
                # no message waiting for us, rest until next attempt
                time.sleep(0.025)
                continue
            # check that it is actually a REQUEST
            if req.verb_type != CSCP1Message.Type.REQUEST:
                self.log_cscp.warning(f"Received malformed request with msg verb: {req.verb}")
                self._cmd_tm.send_reply(
                    f"Received malformed request with msg verb: {req.verb}",
                    CSCP1Message.Type.INVALID,
                )
                continue

            # find a matching callback
            command = req.verb_msg.lower()
            if command not in self._cmds:
                self.log_cscp.warning("Unknown command: %s", command)
                self._cmd_tm.send_reply(f"Unknown command: {command}", CSCP1Message.Type.UNKNOWN)
                continue
            # check whether callback is allowed
            command_cb = getattr(self, command)
            if hasattr(self, "fsm") and hasattr(command_cb, "allowed_states"):
                state = getattr(self, "fsm").state  # noqa: B009
                if command_cb.allowed_states is not None and state not in command_cb.allowed_states:
                    self.log_cscp.warning("Command not allowed in %s state: %s", state.name, req)
                    self._cmd_tm.send_reply(f"Command not allowed in {state.name} state", CSCP1Message.Type.INVALID)
                    continue
            unpack_list = getattr(command_cb, "unpack_list")  # noqa: B009
            # perform the actual callback
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
                    # command not allowed since None returned
                    self.log_cscp.warning("Command not allowed: %s", req)
                    self._cmd_tm.send_reply("Command not allowed", CSCP1Message.Type.INVALID)
                    continue
                res, payload, tags = rv
            except (AttributeError, NotImplementedError) as e:
                self.log_cscp.error("Command failed with %s: %s", e, req)
                self._cmd_tm.send_reply(
                    f"WrongImplementation: {e}",
                    CSCP1Message.Type.NOTIMPLEMENTED,
                    str(e),
                )
                continue
            except TransitionNotAllowed as e:
                self.log_cscp.warning("Transition `%s` not allowed: %s", command, e)
                self._cmd_tm.send_reply(f"Transition not allowed: {e}", CSCP1Message.Type.INVALID, str(e))
                continue
            except (TypeError, ValueError) as e:
                self.log_cscp.error("Command `%s` received wrong argument: %s", command, str(e))
                self._cmd_tm.send_reply(f"Wrong argument: {e}", CSCP1Message.Type.INCOMPLETE, str(e))
                continue
            except Exception as e:
                self.log_cscp.error("Command `%s` failed: %s", command, str(e))
                self._cmd_tm.send_reply(f"Exception: {e}", CSCP1Message.Type.INVALID, str(e))
                continue
            # check the response; empty string means 'missing data/incomplete'
            if res is None:
                self.log_cscp.warning("Command `%s` returned nothing: %s", command, req)
                self._cmd_tm.send_reply("Command returned nothing", CSCP1Message.Type.INCOMPLETE)
                continue
            # finally, assemble a proper response!
            self.log_cscp.debug("Command `%s` succeeded with `%s`: %s", command, res, req)
            try:
                self._cmd_tm.send_reply(res, CSCP1Message.Type.SUCCESS, payload, tags)
            except TypeError as e:
                self.log_cscp.exception("Sending response `%s` failed: %s", res, e)
                self._cmd_tm.send_reply(str(e), CSCP1Message.Type.ERROR, None)
        self.log_cscp.info("CommandReceiver thread shutting down.")
        # shutdown
        self._cmd_tm.close()

    def add_cscp_command(
        self,
        method: str,
        doc: str | None = None,
        allowed_states: list[SatelliteState] | None = None,
        unpack_list: bool = True,
    ) -> None:
        """Add a method to CSCP.

        This is an alternative to using the `@cscp_requestable()` decorator.

        Arguments:

        - method (`str`): name of the method.

        - doc (`str`): a short string providing documentation to the command. If
          no `doc` argument is given, the doc-string of the method will be
          used instead.

        - allowed_states (`list[SatelliteState]`): list of states in which the command is allowed

        """
        call = getattr(self, method)
        if not doc:
            doc = str(call.__doc__)

        # Wrap method in order to set attributes
        @wraps(call)
        def wrapper(*args, **kwargs):
            return call(*args, **kwargs)

        if allowed_states is not None:
            setattr(wrapper, "allowed_states", allowed_states)  # noqa: B010
        setattr(wrapper, "unpack_list", unpack_list)  # noqa: B010

        # Replace method with wrapper
        setattr(self, method, wrapper)

        # Add method to commands
        self._cmds[method] = doc

    @cscp_requestable()
    def get_commands(self, _request: CSCP1Message | None = None) -> tuple[str, Any, dict[str, Any]]:
        """Return all commands supported by the Satellite.

        No payload argument.

        This will include all methods with the `@cscp_requestable()` decorator. The
        doc string of the function will be used to derive the summary and
        payload argument description for each command by using the first and the
        second line of the doc string, respectively (not counting empty lines).

        """
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
    def _get_commands(self, _request: CSCP1Message | None = None) -> tuple[str, Any, dict[str, Any]]:
        """Return all hidden commands supported by the Satellite.

        No payload argument.

        This will include all methods with the @cscp_requestable() decorator starting with an underscore. The
        doc string of the function will be used to derive the summary and payload argument description for
        each command by using the first and the second line of the doc string, respectively (not counting
        empty lines).

        """
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
    def get_name(self, _request: CSCP1Message | None = None) -> tuple[str, Any, dict[str, Any]]:
        """Return the canonical name of the Satellite.

        No payload argument.

        """
        return self.name, None, {}
