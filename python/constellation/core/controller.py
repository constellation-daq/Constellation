"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2
"""

import threading
import time
from collections.abc import Callable
from datetime import datetime
from enum import Enum, auto
from queue import Empty
from typing import Any
from uuid import UUID

import zmq

from .chirp import CHIRPServiceIdentifier, get_uuid
from .chirpmanager import CHIRPManager, DiscoveredService, chirp_callback
from .commandmanager import get_cscp_commands
from .configuration import Configuration
from .controller_configuration import ControllerConfiguration
from .cscp import CommandTransmitter
from .error import debug_log
from .heartbeatchecker import HeartbeatChecker
from .logging import setup_cli_logging
from .monitoring import MonitoringSender
from .protocol.cscp1 import SatelliteState
from .satellite import Satellite
from .util import case_insensitive_dict


class ControllerState(Enum):
    """Global Controller state."""

    NEW = 0x10, "😀"
    INIT = 0x20, "👌"
    ORBIT = 0x30, "🛰"
    RUN = 0x40, "🏃"
    ERROR = 0xF0, "🤯"
    TRANSITIONING = 0x55, "👷"

    def __new__(cls, value: int, name: str) -> Any:
        member = object.__new__(cls)
        member._value_ = value
        member.emoji = name  # type: ignore[attr-defined]
        return member

    def __int__(self) -> int:
        return int(self.value)


class SatelliteUpdate(Enum):
    """Event type for satellite connection callbacks."""

    ADDED = auto()
    REMOVED = auto()


class SatelliteTypeCommLink:
    """A link to a Satellite Type."""

    def __init__(self, satellite_type: str):
        self._satellite_type = satellite_type

    def __str__(self) -> str:
        """Convert to type name."""
        return self._satellite_type


class SatelliteCommLink(SatelliteTypeCommLink):
    """A link to a Satellite."""

    def __init__(self, satellite_name: str, satellite_type: str):
        self._satellite_name = satellite_name
        self._uuid = str(get_uuid(f"{satellite_type}.{satellite_name}"))
        super().__init__(satellite_type)

    def __str__(self) -> str:
        """Convert to canonical name."""
        return f"{self._satellite_type}.{self._satellite_name}"

    def _repr_pretty_(self, p: Any, cycle: bool) -> None:  # p is a pretty printer from IPython
        if cycle:
            # this is not a container, so this should not happen!
            pass
        else:
            p.text(f"SatelliteCommLink(type={self._satellite_type}, name={self._satellite_name})")


class SatelliteArray:
    """Provide object-oriented control of connected Satellites."""

    def __init__(
        self,
        group: str,
        handler: Callable[[str, Any, str | None, str | None], tuple[str, Any, dict[str, Any] | None]],
    ):
        self.group = group
        self._handler = handler
        # initialize with the commands known to any CSCP Satellite
        self._add_cmds(self, self._handler, get_cscp_commands(Satellite))
        self._satellites: dict[str, SatelliteCommLink] = {}

    @property
    def satellites(self) -> dict[str, SatelliteCommLink]:
        """Return the dict of Satellites names and their SatelliteCommLink."""
        return self._satellites

    def get_satellite(self, sat_type: str, sat_name: str) -> SatelliteCommLink:
        """Return a link to a Satellite given by its type and name."""
        return self._satellites[f"{sat_type}.{sat_name}"]

    def _add_type(
        self, name: str, commands: dict[str, Any], hidden_commands: dict[str, str] | None = None
    ) -> SatelliteTypeCommLink:
        """Add a new satellite type to the array."""
        if not hidden_commands:
            hidden_commands = {}
        try:
            cl: SatelliteTypeCommLink = getattr(self, name)
            return cl
        except AttributeError:
            pass
        # add attributes now
        cl = SatelliteTypeCommLink(name)
        self._add_cmds(cl, self._handler, commands)
        self._add_cmds(cl, self._handler, hidden_commands)
        setattr(self, name, cl)
        return cl

    def _add_satellite(
        self,
        satellite_name: str,
        satellite_type: str,
        commands: dict[str, str],
        hidden_commands: dict[str, str] | None = None,
    ) -> SatelliteCommLink:
        """Add a new Satellite."""
        if not hidden_commands:
            hidden_commands = {}
        try:
            cl: SatelliteTypeCommLink = getattr(self, satellite_type)
        except AttributeError:
            cl = self._add_type(satellite_type, commands, hidden_commands)
        sat: SatelliteCommLink = SatelliteCommLink(satellite_name, satellite_type)
        self._add_cmds(sat, self._handler, commands)
        self._add_cmds(sat, self._handler, hidden_commands)
        setattr(cl, self._sanitize_name(satellite_name), sat)
        self._satellites[f"{satellite_type}.{satellite_name}"] = sat
        return sat

    def _remove_satellite(self, uuid: str) -> None:
        """Remove a Satellite."""
        satellite_name, satellite_type = self._get_name_from_uuid(uuid)
        # remove attribute
        delattr(getattr(self, satellite_type), self._sanitize_name(satellite_name))
        # clear from dict
        self._satellites.pop(f"{satellite_type}.{satellite_name}")

    def _get_name_from_uuid(self, uuid: str) -> tuple[str, str]:
        s = [sat for sat in self._satellites.values() if sat._uuid == uuid]
        if not s:
            raise KeyError("No Satellite with that UUID known.")
        satellite_name = s[0]._satellite_name
        satellite_type = s[0]._satellite_type
        return satellite_name, satellite_type

    def _add_cmds(
        self,
        obj: Any,
        handler: Callable[[str, Any, str | None, str | None], tuple[str, Any, dict[str, Any] | None]],
        cmds: dict[str, str],
    ) -> None:
        try:
            sat_name = obj._satellite_name
        except AttributeError:
            sat_name = None
        try:
            sat_type = obj._satellite_type
        except AttributeError:
            sat_type = None
        for cmd, doc in cmds.items():
            w = CommandWrapper(handler, sat_name=sat_name, sat_type=sat_type, cmd=cmd)
            # add docstring
            w.call.__func__.__doc__ = doc
            setattr(obj, cmd, w.call)

    def _sanitize_name(self, name: str) -> str:
        """Remove characters not suited for Python methods from names."""
        return name.replace("-", "_").replace(".", "_")

    def _repr_pretty_(self, p: Any, cycle: bool) -> None:
        if cycle:
            # not a container, so this should never be true
            pass
        else:
            with p.group(len(self.group) + 2, f"{self.group.capitalize()}([", "])"):
                for idx, item in enumerate(self.satellites.keys()):
                    if idx:
                        p.text(",")
                        p.breakable()
                    p.pretty(item)


class CommandWrapper:
    """Class to wrap command calls.

    Allows to mimic the signature of the Satellite command being wrapped.
    """

    def __init__(
        self,
        handler: Callable[[str, Any, str | None, str | None], tuple[str, Any, dict[str, Any] | None]],
        sat_name: str | None,
        sat_type: str | None,
        cmd: str,
    ):
        """Initialize with fcn as a partial() call."""
        self.fcn = handler
        self.sat_name = sat_name
        self.sat_type = sat_type
        self.cmd = cmd

    def call(self, payload: Any = None) -> tuple[str, Any, dict[str, Any] | None]:
        """Perform call. This doc string will be overwritten."""
        return self.fcn(self.cmd, payload, self.sat_type, self.sat_name)


class SatelliteResponse:
    """Class to hold the response of a Satellite to a CSCP command."""

    def __init__(self) -> None:
        self.success: bool = True
        self.meta: dict[str, Any] | None = None
        self.payload: Any = None
        self.errmsg: str = ""
        self.msg: str = ""

    def __str__(self) -> str:
        """Pretty-print request."""
        if not self.success:
            return f"Failed: {self.errmsg}"
        s = "'{}'{}{}"
        return s.format(
            self.msg,
            f" with a payload {self.payload}" if self.payload else "",
            f" with a meta header {self.meta}" if self.meta else "",
        )

    def _repr_pretty_(self, p: Any, cycle: bool) -> None:
        if cycle:
            # not a container, so this should never be true
            return
        if not self.success:
            p.text(f"SatelliteResponse(success=False, errmsg='{self.errmsg}')")
            return
        with p.group(19, "SatelliteResponse(", ")"):
            p.breakable(sep="")
            p.text(f"msg='{self.msg}'")
            if isinstance(self.payload, dict):
                p.text(", ")
                p.breakable()
                with p.group(9, "payload={", "}"):
                    for idx, (key, item) in enumerate(self.payload.items()):
                        if idx:
                            p.text(",")
                            p.breakable()
                        p.text(f'"{key}": ')
                        p.pretty(item)
            else:
                if self.payload:
                    p.text(f", payload={str(self.payload)}")
            if self.meta:
                p.text(", ")
                p.breakable()
                with p.group(4, "meta={", "}"):
                    for idx, (key, item) in enumerate(self.meta.items()):
                        if idx:
                            p.text(",")
                            p.breakable()
                        p.text(f'"{key}": ')
                        p.pretty(item)


class BaseController(MonitoringSender, CHIRPManager, HeartbeatChecker):
    """Simple controller class to send commands to a Constellation."""

    def __init__(self, group: str, **kwargs: Any) -> None:
        """Initialize values.

        Arguments:
        - name ::  name of controller
        - group ::  group of controller
        - interface :: the interface to connect to
        """
        super().__init__(group=group, **kwargs)

        self._transmitters: dict[str, CommandTransmitter] = {}
        # lookup table for uuids to (type, name) tuple
        self._uuid_lookup: dict[str, tuple[str, str]] = {}

        self._constellation = SatelliteArray(group, self.command)

        # Override logger with operator topic
        self.log = self.get_logger("OP")

        super()._add_com_thread()
        super()._start_com_threads()

        # set up thread to handle incoming tasks (e.g. CHIRP discoveries)
        self._task_handler_event = threading.Event()
        self._task_handler_thread = threading.Thread(target=self._run_task_handler, daemon=True)
        self._task_handler_thread.start()

        # register monitoring service
        self.register_offer(CHIRPServiceIdentifier.MONITORING, self.mon_port)
        self.emit_offers()

        # wait for threads to be ready
        time.sleep(0.2)
        self.request(CHIRPServiceIdentifier.CONTROL)
        time.sleep(0.2)
        self.request(CHIRPServiceIdentifier.HEARTBEAT)
        time.sleep(0.1)

    @property
    def states(self) -> case_insensitive_dict[SatelliteState]:
        """Return an up-to-date dictionary of connected Satellite's state.

        Based on heartbeat information.

        """
        return self.heartbeat_states

    @property
    def last_state_change(self) -> case_insensitive_dict[datetime]:
        """Return a dictionary of connected Satellite's last state change.

        Based on heartbeat information.

        """
        return self.heartbeat_state_changes

    @property
    def state(self) -> ControllerState:
        """Return the global state of all connected Satellite's state.

        Based on heartbeat information.

        """
        if len(self.states) == 0:
            return ControllerState.NEW
        # if any Satellite is in ERROR, the global state will be in ERROR as well
        if any(
            state in self.states.values()
            for state in [
                SatelliteState.ERROR,
                SatelliteState.DEAD,
                SatelliteState.SAFE,
            ]
        ):
            return ControllerState.ERROR
        # any transitional states?
        if any(
            state in self.states.values()
            for state in [
                SatelliteState.initializing,
                SatelliteState.launching,
                SatelliteState.landing,
                SatelliteState.reconfiguring,
                SatelliteState.starting,
                SatelliteState.stopping,
                SatelliteState.interrupting,
            ]
        ):
            return ControllerState.TRANSITIONING
        # now go by lowest steady state of any satellite:
        for target in [
            ControllerState.NEW,
            ControllerState.INIT,
            ControllerState.ORBIT,
            ControllerState.RUN,
        ]:
            if any(state.value == target.value for state in self.states.values()):
                return target
        # something went wrong..?
        return ControllerState.ERROR

    @property
    def status(self) -> str:
        """Return the global status summary of the Constellation.

        Based on heartbeat information.

        """
        res = []
        sats = []
        for state in SatelliteState:
            sats = [sat for sat, sat_state in self.states.items() if sat_state == state]
            if sats:
                res.append(f"{len(sats)} Satellite{'s are' if len(sats) > 1 else ' is'} in {state.name}")
        if len(self.states) != len(self.constellation.satellites):
            miss = len(self.constellation.satellites) - len(self.states)
            res.append(f"{miss} connected Satellite{'s are' if len(sats) > 1 else ' is'} missing heartbeat information")
        prefix = f"{len(self.constellation.satellites)} connected: "
        if len(res) == 1:
            return prefix + "All " + res[0]
        return prefix + ", ".join(res)

    def get_last_state_change(self, satellites: list[str]) -> dict[str, datetime]:
        """Return a dictionary of selected connected Satellite's last state change."""
        last_state_change: dict[str, datetime] = {}
        for satellite in satellites:
            if satellite not in self.last_state_change:
                raise Exception(f"Satellite {satellite} not known to controller")
            last_state_change[satellite] = self.last_state_change[satellite]
        return last_state_change

    def await_state(self, target: SatelliteState, timeout: int = 60) -> None:
        """Blocks until the desired global state of the connected satellites is reached."""
        self.log.info("Awaiting global state %s", target)
        start = time.time()
        while not all([state == target for state in self.states.values()]):
            if time.time() - start > timeout:
                raise Exception(f"Timeout after {timeout}s while waiting for state {target.name}")
            if any([state == SatelliteState.ERROR for state in self.states.values()]):
                raise Exception(f"ERROR state detected while waiting for state {target.name}")
            time.sleep(0.1)

    def await_state_change(self, target: SatelliteState, last_state_change: dict[str, datetime], timeout: int = 60) -> None:
        """Blocks until the desired global state of the connected satellites is reached with check for state changes."""

        # Copy dict so that we can modify it for the next iteration
        last_state_change_copy = last_state_change.copy()

        start = time.time()

        while last_state_change:
            # Check that last extrasystole is more recent than the timestamp given in the dict
            for satellite, last_change in last_state_change.items():
                new_last_change = self.last_state_change[satellite]
                if new_last_change > last_change:
                    # New extrasystole found, remove from map for next iteration
                    del last_state_change_copy[satellite]
                    self.log.trace("State change registered for %s", satellite)

            # Copy dict with removed entries for next iteration
            last_state_change = last_state_change_copy.copy()

            # Check for timeout
            if time.time() - start > timeout:
                raise Exception(
                    f"Timeout after {timeout}s while waiting for state {target.name}: "
                    f"{last_state_change.keys()} never changed state"
                )

            time.sleep(0.1)

        # Once all sent an extrasystole, await state as usual with remaining timeout
        remaining_timeout = timeout - round(time.time() - start)
        self.await_state(target, remaining_timeout)

    def await_satellites(self, satellites: list[str], timeout: int = 60) -> None:
        """Blocks until all desired satellites are connected."""
        self.log.info("Awaiting %d satellites", len(satellites))
        start = time.time()
        while not set(satellites).issubset(self._constellation.satellites.keys()):
            if time.time() - start > timeout:
                not_found = ", ".join(set(satellites).difference(self._constellation.satellites.keys()))
                raise Exception(
                    f"Timeout after {timeout}s while waiting for {len(satellites)} satellites: could not find {not_found}"
                )
            time.sleep(0.1)

    @property
    def constellation(self) -> SatelliteArray:
        """Returns the currently active SatelliteArray of controlled Satellites."""
        return self._constellation

    @debug_log
    @chirp_callback(CHIRPServiceIdentifier.CONTROL)
    def _add_satellite_callback(self, service: DiscoveredService) -> None:
        """Callback method connecting to satellite."""
        if not service.alive:
            self._remove_satellite(service)
        else:
            self._add_satellite(service)

    @debug_log
    def _add_satellite_heartbeat(self, canonical_name: str, uuid: UUID, ct: CommandTransmitter) -> None:
        """Callback method registering satellite's heartbeat."""

        # get current state and timestamp of last change
        msg = ct.request_get_response("get_state")
        init_state = {"state": SatelliteState[msg.verb_msg], "last_changed": msg.tags["last_changed"]}

        # wait until heartbeat service is discovered
        timeout = 1.0
        while timeout > 0.0 and uuid not in [
            hbservice.host_uuid for hbservice in self.get_discovered(CHIRPServiceIdentifier.HEARTBEAT)
        ]:
            time.sleep(0.05)
            timeout -= 0.05

        # check if heartbeat service for satellite is known already
        hb_registered = False
        for hbservice in self.get_discovered(CHIRPServiceIdentifier.HEARTBEAT):
            if hbservice.host_uuid == uuid:
                if self.heartbeat_host_is_registered(uuid):
                    self.unregister_heartbeat_host(uuid)
                self.register_heartbeat_host(
                    host=uuid,
                    address=f"tcp://{hbservice.address}:{hbservice.port}",
                    name=canonical_name,
                    init_state=init_state,
                )
                hb_registered = True
                break

        if not hb_registered:
            self.log.warning("Could not find HEARTBEAT service for %s, state information not available", canonical_name)

    def _add_satellite(self, service: DiscoveredService) -> None:
        self.log.debug("Adding Satellite %s", service)
        if str(service.host_uuid) in self._uuid_lookup:
            self.log.error(
                "Satellite with name '%s.%s' already connected! Please ensure "
                "unique Satellite names or you will not be able to communicate with all!",
                *self._uuid_lookup[str(service.host_uuid)],
            )
        # create socket
        try:
            socket = self.context.socket(zmq.REQ)
        except zmq.ZMQError as e:
            if "Too many open files" in e.strerror:
                self.log.error(
                    "System reports too many open files: cannot open further connections.\n"
                    "Please consider increasing the limit of your OS."
                    "On Linux systems, use 'ulimit' to set a higher value."
                )
            raise e
        socket.connect("tcp://" + service.address + ":" + str(service.port))
        # Set linger period for socket shutdown to avoid long hangs shutting
        # down [ms]
        socket.setsockopt(zmq.LINGER, 2000)
        # Set maximum time before a recv operation returns with EAGAIN [ms]
        socket.setsockopt(zmq.RCVTIMEO, 5000)
        ct = CommandTransmitter(self.name, socket)
        self.log.debug("Connecting to %s, address %s on port %s...", service.host_uuid, service.address, service.port)
        try:
            # get list of commands
            msg = ct.request_get_response("get_commands")
            hidden_cmds = {}

            try:
                # also register hidden commands:
                hidden_cmds = ct.request_get_response("_get_commands").payload
            except RuntimeError:
                pass

            # extract canonical name
            sat_type, sat_name = msg.sender.split(".", maxsplit=1)
            canonical_name = f"{sat_type}.{sat_name}"

            # add satellite heartbeat
            self._add_satellite_heartbeat(canonical_name, service.host_uuid, ct)

            # add satellite to constellation
            sat = self._constellation._add_satellite(sat_name, sat_type, msg.payload, hidden_cmds)
            if sat._uuid != str(service.host_uuid):
                self.log.warning("UUIDs do not match: expected %s but received %s", sat._uuid, str(service.host_uuid))
            uuid = str(service.host_uuid)
            self._uuid_lookup[uuid] = (sat_type, sat_name)
            self._transmitters[uuid] = ct

            self.log.info("Satellite %s connected", canonical_name)
            self._on_satellite_update(canonical_name, SatelliteUpdate.ADDED)
        except RuntimeError as e:
            self.log.error("Could not add Satellite %s: %s", service.host_uuid, repr(e))

    def _remove_satellite(self, service: DiscoveredService) -> None:
        satellite_name, satellite_type = None, None
        # departure
        uuid = str(service.host_uuid)
        try:
            satellite_name, satellite_type = self._constellation._get_name_from_uuid(uuid)
            self._constellation._remove_satellite(uuid)
        except KeyError:
            pass
        if satellite_name is not None and satellite_type is not None:
            self._on_satellite_update(f"{satellite_type}.{satellite_name}", SatelliteUpdate.REMOVED)
        if self.heartbeat_host_is_registered(service.host_uuid):
            self.unregister_heartbeat_host(service.host_uuid)
        self.log.debug("Departure of %s, known as %s.%s", service.host_uuid, satellite_type, satellite_name)
        try:
            ct = self._transmitters[uuid]
            ct.close()
            self._transmitters.pop(uuid)
            self._uuid_lookup.pop(uuid)
        except KeyError:
            pass

    def _on_satellite_update(self, name: str, update_type: SatelliteUpdate) -> None:
        """Called when a satellite connects or disconnects. Override in subclass."""
        pass

    def _hb_failure(self, name: str, state: SatelliteState) -> None:
        """Callback for Satellites failing to send heartbeats."""
        self.log.critical("%s has entered %s", name, state.name)

    def command(self, cmd: str, payload: Any, sat_type: str | None, sat_name: str | None) -> Any:
        """Wrapper for _command_satellite function. Handle sending commands to all hosts"""
        targets = []
        # figure out whether to send command to satellite, satellite type or whole Constellation
        if not sat_type:
            targets = list(self._constellation.satellites.values())
            self.log.debug("Sending %s to all %s connected satellites", cmd, len(targets))
        elif not sat_name:
            targets = [sat for sat in self._constellation.satellites.values() if sat._satellite_type == sat_type]
            self.log.debug("Sending %s to all %s connected satellites of type %s", cmd, len(targets), sat_type)
        else:
            targets = [self._constellation.get_satellite(sat_type, sat_name)]
            self.log.debug("Sending %s to Satellite %s.", cmd, targets[0])

        res: dict[str, SatelliteResponse] = {}
        for target in targets:
            self.log.debug("Host %s send command %s...", target, cmd)
            # The payload to set of (known) command can be pre-processed
            # allowing using more complex objects as arguments and a more
            # convenient CLI user experience without impacting the protocol
            # specs. Here, we translate to what the protocol requires.
            p = self._preprocess_payload(payload, target._uuid, cmd)
            sat_response = SatelliteResponse()
            ret_msg = None
            try:
                ret_msg = self._transmitters[target._uuid].request_get_response(command=cmd, payload=p)
            except KeyError:
                self.log.error("Command %s failed for %s: No transmitter available", cmd, target)
                sat_response.success = False
                sat_response.errmsg = "No transmitter available"
            except RuntimeError as e:
                self.log.error("Command %s failed for %s: %s", cmd, target, repr(e))
                sat_response.success = False
                sat_response.errmsg = repr(e)
            if ret_msg:
                self.log.debug("%s responded: %s", ret_msg.sender, ret_msg.verb)
                if ret_msg.tags:
                    self.log.debug("    header: %s", ret_msg.tags)
                if ret_msg.payload:
                    self.log.debug("    payload: %s", ret_msg.payload)
                sat_response.msg = ret_msg.verb_msg
                sat_response.payload = ret_msg.payload
                sat_response.meta = ret_msg.tags
            if sat_name:
                # simplify return value for single satellite
                return sat_response
            # append
            res[str(target)] = sat_response
        return res

    def _preprocess_payload(self, payload: Any, uuid: str, cmd: str) -> Any:
        """Pre-processes payload for specific commands."""
        if cmd in ("initialize", "reconfigure"):
            # payload needs to be a Configuration, but we want to allow to
            # supply a full config or a plain dict -- adjust here
            if not isinstance(payload, Configuration):
                if isinstance(payload, dict):
                    payload = Configuration(payload)
                elif isinstance(payload, ControllerConfiguration):
                    sat_type, sat_name = self._uuid_lookup[uuid]
                    payload = payload.get_satellite_configuration(f"{sat_type}.{sat_name}")
                else:
                    raise RuntimeError("Payload needs to be a dictionary, configuration or controller configuration")
            return payload._dictionary
        return payload

    def _run_task_handler(self) -> None:
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

    def reentry(self) -> None:
        """Stop the controller."""
        self.log.debug("Stopping controller")

        if getattr(self, "_task_handler_event", None):
            self._task_handler_event.set()

        super().reentry()

        try:
            for _name, cmd_tm in self._transmitters.items():
                cmd_tm.close()
        except Exception:
            # ignore errors; this avoids spurious error messages if e.g. the
            # initialization of the class fails
            pass

        if getattr(self, "_task_handler_event", None):
            self._task_handler_thread.join(timeout=1)

    def _repr_pretty_(self, p: Any, _cycle: bool) -> None:
        nsat = len(self.constellation.satellites)
        p.text(f"Controller(group='{self.group}') for {nsat} Satellites, current state is {self.state.name}")


class ScriptableController(BaseController):
    """Simple controller class to which initializes logging and networking."""

    def __init__(
        self,
        group: str,
        log_level: str = "INFO",
        name: str = "ScriptableController",
        interface: list[str] | None = None,
    ) -> None:
        # Initialize logging first
        setup_cli_logging(log_level)

        # Initialize BaseController
        super().__init__(group=group, name=name, interface=interface)
