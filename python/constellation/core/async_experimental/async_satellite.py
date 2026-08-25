"""
SPDX-FileCopyrightText: 2026 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2

This module provides the class for an async Constellation Satellite.
"""

import asyncio
import re
import time
from collections import defaultdict
from collections.abc import Callable
from typing import Any

import psutil
from statemachine.exceptions import TransitionNotAllowed

from constellation.core import __version__
from constellation.core.async_experimental.async_chirp import (
    AsyncCHIRPManager,
    CHIRPEvent,
    DiscoveredService,
)
from constellation.core.async_experimental.async_cscp_receiver import AsyncCSCPReceiver
from constellation.core.async_experimental.async_heartbeat import AsyncHeartbeatChecker
from constellation.core.async_experimental.async_heartbeatsender import AsyncHeartbeatSenderMixin
from constellation.core.base import ConstellationArgumentParser
from constellation.core.chirp import CHIRPServiceIdentifier
from constellation.core.chp import CHPRole
from constellation.core.commandmanager import cscp_requestable
from constellation.core.configuration import Configuration, ConfigurationGroup, InvalidValueError
from constellation.core.error import debug_log, handle_error
from constellation.core.fsm import SatelliteFSM
from constellation.core.monitoring import MonitoringSender
from constellation.core.protocol.cscp1 import SatelliteState, is_valid_canonical_name


class AsyncSatelliteStateHandler(AsyncHeartbeatChecker, AsyncCSCPReceiver):
    """Async satellite state handler.

    Manages the satellite FSM with asyncio. Transition methods use
    asyncio.to_thread for blocking user callbacks, and an asyncio.Queue
    for task dispatch.
    """

    def __init__(self, **kwds: Any) -> None:
        self.fsm = SatelliteFSM()
        super().__init__(**kwds)

        self.log_fsm = self.get_logger("FSM")

        self.conditions: dict[SatelliteState, list[str]] = defaultdict(list)

        # State task and stop event for the RUN state
        self._state_task: asyncio.Task | None = None
        self._state_stop_event: asyncio.Event | None = None

    def register_state_callback(self, callback_id: str, callback: Callable[[SatelliteState], None]) -> None:
        """Register a callback for state changes."""
        self.fsm.state_callbacks[callback_id] = callback

    def unregister_state_callback(self, callback_id: str) -> None:
        """Unregister a callback for state changes."""
        if callback_id in self.fsm.state_callbacks:
            del self.fsm.state_callbacks[callback_id]

    @debug_log
    @cscp_requestable(unpack_list=False)
    def initialize(self, config_dict: dict) -> tuple[str, Any, dict[str, Any]]:
        """Initiate 'initialize' state transition via a CSCP request."""
        config = Configuration(config_dict)

        self.conditions = defaultdict(list)

        if "_conditions" in config:
            config_conditions = config.get_section("_conditions")

            for transition in [
                SatelliteState.initializing,
                SatelliteState.launching,
                SatelliteState.landing,
                SatelliteState.starting,
                SatelliteState.stopping,
            ]:
                key = f"require_{transition.name}_after"
                if key in config_conditions:
                    satellites = config_conditions.get_array(key, element_type=str)
                    for satellite in satellites:
                        if not is_valid_canonical_name(satellite):
                            raise InvalidValueError(config_conditions, key, f"{satellite} is not a valid canonical name")
                        if satellite == self.name:
                            raise InvalidValueError(config_conditions, key, "Satellite cannot depend on itself")
                        if satellite not in self.heartbeat_states or self.heartbeat_states[satellite] == SatelliteState.DEAD:
                            raise InvalidValueError(
                                config_conditions, key, f"Dependent remote satellite {satellite} is not present"
                            )

                    self.conditions[transition] = satellites
                    self.log_fsm.debug(f"Registered remote condition {transition.name} with {self.conditions[transition]}")

            self._conditional_transition_timeout = config_conditions.get_int("transition_timeout", 30, min_val=0)

        return self._enqueue_transition("initialize", config, run_in_task=False)

    @debug_log
    @cscp_requestable()
    def launch(self) -> tuple[str, Any, dict[str, Any]]:
        """Initiate launch state transition via a CSCP request."""
        return self._enqueue_transition("launch", None, run_in_task=False)

    @debug_log
    @cscp_requestable()
    def land(self) -> tuple[str, Any, dict[str, Any]]:
        """Initiate landing state transition via a CSCP request."""
        return self._enqueue_transition("land", None, run_in_task=False)

    @debug_log
    @cscp_requestable(unpack_list=False)
    def start(self, run_identifier: str) -> tuple[str, Any, dict[str, Any]]:
        """Initiate start state transition via a CSCP request."""
        if not re.match(r"^[\w-]+$", run_identifier):
            raise ValueError("Run identifier contains invalid characters")
        return self._enqueue_transition("start", run_identifier, run_in_task=True)

    @debug_log
    @cscp_requestable()
    def stop(self) -> tuple[str, Any, dict[str, Any]]:
        """Initiate stop state transition via a CSCP request."""
        return self._enqueue_transition("stop", None, run_in_task=False)

    @debug_log
    def reconfigure(self, config_dict: dict) -> tuple[str, Any, dict[str, Any]]:
        """Initiate reconfigure state transition via a CSCP request."""
        if not hasattr(self, "do_reconfigure"):
            raise NotImplementedError("Reconfigure not supported: missing function 'do_reconfigure'")
        partial_config = Configuration(config_dict)
        return self._enqueue_transition("reconfigure", partial_config, run_in_task=False)

    @debug_log
    @cscp_requestable()
    def _interrupt(self) -> tuple[str, Any, dict[str, Any]]:
        """Initiate interrupt state transition via a CSCP request.

        This is intended for debugging purposes only and should not be called in normal operation.
        """
        return self._enqueue_transition("interrupt", None, run_in_task=False)

    @debug_log
    @cscp_requestable()
    def _failure(self) -> tuple[str, Any, dict[str, Any]]:
        """Enter error state transition via a CSCP request.

        This is intended for debugging purposes only and should not be called in normal operation.
        """
        return self._enqueue_transition("failure", None, run_in_task=False)

    def _enqueue_transition(self, target: str, payload: Any, run_in_task: bool) -> tuple[str, Any, dict[str, Any]]:
        """Validate FSM transition and enqueue the work for the run loop.

        The actual work is placed on the async task queue so it runs
        from the main loop coroutine, preserving single-threaded
        dispatch.
        """
        self.log_fsm.debug("State transition %s requested", target)
        getattr(self.fsm, target)(f"{target.capitalize()} requested")

        self.log_fsm.status("State transition %s initiated", target)
        transit_fcn = getattr(self, f"_wrap_{target}")

        if run_in_task:
            self._async_task_queue.put_nowait((self._start_transition_task, [transit_fcn, payload]))
        else:
            self._async_task_queue.put_nowait((self._start_transition, [transit_fcn, payload]))
        return "transitioning", target, {}

    @handle_error
    async def _satisfy_remote_conditions_async(self) -> None:
        """Wait for configured remote satellites to reach the required state."""
        self.log_fsm.debug("Awaiting remote conditions for state transition %s", self.fsm.current_state)

        if self.fsm.current_state.value not in self.conditions.keys():
            self.log_fsm.trace("No condition configured for state transition %s", self.fsm.current_state)
            return

        timeout = self._conditional_transition_timeout
        timeout_start = time.time()
        dbg_msg_logged: set[str] = set()

        while True:
            satisfied = True
            for name in self.conditions[self.fsm.state]:
                if name not in self.heartbeat_states or self.heartbeat_states[name] == SatelliteState.DEAD:
                    error_message = f"Dependent remote satellite {name} not present"
                    self.fsm.status = error_message
                    raise RuntimeError(error_message)

                if self.heartbeat_states[name] == SatelliteState.ERROR:
                    error_message = f"Dependent remote satellite {name} reports state {self.heartbeat_states[name]}"
                    self.fsm.status = error_message
                    raise RuntimeError(error_message)

                if not self.fsm.state.transitions_to(self.heartbeat_states[name]):
                    msg = f"Awaiting state from {name}, currently: {self.heartbeat_states[name]}"
                    if msg not in dbg_msg_logged:
                        self.log_fsm.debug(msg)
                        dbg_msg_logged.add(msg)
                    self.fsm.set_status(msg)
                    satisfied = False
                    break

            if satisfied:
                self.log_fsm.debug("Satisfied with all remote conditions, continuing")
                return

            if time.time() > timeout_start + timeout:
                error_message = "Could not satisfy remote conditions within timeout"
                self.fsm.status = error_message
                raise RuntimeError(error_message)

            await asyncio.sleep(0.01)

    @debug_log
    async def _start_transition(self, fcn: Callable[[Any], str], payload: Any) -> None:
        """Start a transition and advance FSM for transitional states."""
        await self._satisfy_remote_conditions_async()

        res = await asyncio.to_thread(fcn, payload)
        if not res:
            res = "Transition completed!"
        try:
            prev = self.fsm.state.name
            self.fsm.complete(res)
            now = self.fsm.state.name
            self.log_fsm.status(f"State transition to steady state completed ({prev} -> {now}).")
        except TransitionNotAllowed:
            if self.fsm.state != SatelliteState.ERROR:
                self.fsm.status = res

    @debug_log
    async def _start_transition_task(self, fcn: Callable[[Any], str], payload: Any) -> None:
        """Start a transition in a background asyncio.Task.

        Used for the RUN state where the user's do_run callback runs
        concurrently alongside the CSCP receiver. The stop command
        signals the _state_stop_event which the satellite's
        stop_requested method checks.
        """
        await self._satisfy_remote_conditions_async()

        self._state_stop_event = asyncio.Event()

        async def _run_in_background() -> None:
            try:
                res = await asyncio.to_thread(fcn, payload)
                if not res:
                    res = "Transition completed!"
                if self._state_stop_event and self._state_stop_event.is_set():
                    self._state_stop_event = None
                    return
                self._state_stop_event = None
                try:
                    prev = self.fsm.state.name
                    self.fsm.complete(res)
                    now = self.fsm.state.name
                    self.log_fsm.status(f"State transition to steady state completed ({prev} -> {now}).")
                except TransitionNotAllowed:
                    if self.fsm.state != SatelliteState.ERROR:
                        self.fsm.status = res
            except Exception as e:
                self._state_stop_event = None
                err_msg = f"Transition task failed: {e}"
                self.log_fsm.critical(err_msg)
                try:
                    self.fsm.failure(err_msg)
                    self._wrap_failure(err_msg)
                except Exception:
                    pass

        self._state_task = asyncio.create_task(_run_in_background())

    @cscp_requestable()
    def get_state(self) -> tuple[str, Any, dict[str, Any]]:
        """Return the current state of the Satellite."""
        payload = self.fsm.state.value
        meta = {
            "last_changed": self.fsm.last_changed,
            "last_changed_iso": self.fsm.last_changed.isoformat(),
        }
        return self.fsm.state.name, payload, meta

    @cscp_requestable()
    def get_status(self) -> tuple[str, Any, dict[str, Any]]:
        """Get a string describing the current status of the Satellite."""
        return self.fsm.status, None, {}


class AsyncSatellite(
    MonitoringSender,
    AsyncSatelliteStateHandler,
    AsyncCHIRPManager,
    AsyncHeartbeatSenderMixin,
):
    """Async Constellation Satellite.

    The CSCP receiver, heartbeat sender, heartbeat checker, CHIRP,
    and monitoring all run as concurrent tasks on the event loop.
    """

    def __init__(
        self,
        name: str,
        group: str,
        cmd_port: int = 0,
        hb_port: int = 0,
        mon_port: int = 0,
        interface: list[str] | None = None,
    ) -> None:
        super().__init__(
            name=name,
            group=group,
            cmd_port=cmd_port,
            hb_port=hb_port,
            mon_port=mon_port,
            interface=interface,
        )

        self.log_satellite = self.get_logger("CTRL")

        self.run_identifier: str = ""
        self.run_degraded: bool = False
        self._config = Configuration()

        self._async_task_queue: asyncio.Queue = asyncio.Queue()

        # Register CHIRP services and request heartbeats
        self.register_service(CHIRPServiceIdentifier.CONTROL, self.cmd_port)
        self.register_service(CHIRPServiceIdentifier.HEARTBEAT, self.hb_port)
        self.register_service(CHIRPServiceIdentifier.MONITORING, self.mon_port)

        if hasattr(self, "do_reconfigure"):
            self.add_cscp_command("reconfigure", allowed_states=[SatelliteState.ORBIT], unpack_list=False)

        self.register_metric("RUN_ID", "", "Current run identifier. Updated when changed.")
        self.register_metric("STATE", "", "Current satellite state. Updated when changed.")
        self.register_scheduled_metric(
            "CPU_LOAD_AVG", "%", "CPU load average.", 10, lambda: 100 * psutil.getloadavg()[0] / (psutil.cpu_count() or 1)
        )
        self.register_scheduled_metric(
            "MEM_AVAIL", "MiB", "Available memory in Megabytes.", 10, lambda: psutil.virtual_memory().available / 1024 / 1024
        )
        self.register_state_callback("telemetry", self._stat_state)

        self.register_chirp_callback("satellite_heartbeat", self._on_heartbeat_service)

        self.log_satellite.info(f"Satellite {self.name}, version {__version__} ready to launch!")

    def _on_heartbeat_service(self, event: CHIRPEvent, service: DiscoveredService) -> None:
        """Handle HEARTBEAT service discovery."""
        if service.service_id != CHIRPServiceIdentifier.HEARTBEAT:
            return
        if event == CHIRPEvent.SERVICE_CONNECTED:
            self.log_satellite.debug(f"Registering new host for heartbeats at {service.addresses[0]}:{service.port}")
            name = service.host_name if hasattr(service, "host_name") else f"Unknown-{str(service.host_id)[:8]}"
            self.register_heartbeat_host(service.host_id, service.addresses[0], service.port, name)
        elif event == CHIRPEvent.SERVICE_DISCONNECTED:
            self.log_satellite.debug("Unregistering host for heartbeats")
            self.unregister_heartbeat_host(service.host_id)

    def _stat_state(self, state: SatelliteState) -> None:
        """Metric for the current satellite state."""
        self.stat("STATE", state.name)

    async def run_satellite(self) -> None:
        """Async satellite event loop.

        Starts all communication tasks (CSCP, heartbeat, CHIRP,
        monitoring) and processes transition callbacks from the async
        task queue until shutdown is requested.
        """
        stop = asyncio.Event()

        # Emit CHIRP offers and request heartbeats
        self.emit_offers()
        self.request(CHIRPServiceIdentifier.HEARTBEAT)

        # Register communication tasks
        if not self._com_task_factories:
            self._add_com_task()

        # Launch all background tasks
        com_tasks = [asyncio.create_task(factory(stop)) for factory in self._com_task_factories]

        # Main task processing loop
        try:
            while not stop.is_set():
                try:
                    task = await asyncio.wait_for(self._async_task_queue.get(), timeout=0.5)
                    callback = task[0]
                    args = task[1]
                    try:
                        self.log_satellite.trace(f"Executing {callback}")
                        result = callback(*args)
                        if asyncio.iscoroutine(result):
                            await result
                    except Exception as e:
                        self.log_satellite.exception(
                            "Caught exception handling task '%s' with args '%s': %s",
                            callback,
                            args,
                            repr(e),
                        )
                except TimeoutError:
                    pass
                except asyncio.CancelledError:
                    break
        except KeyboardInterrupt:
            print()
            self.log_satellite.warning("Satellite caught KeyboardInterrupt, shutting down.")
            self.shutdown()

        # Signal all background tasks to stop
        stop.set()
        await asyncio.gather(*com_tasks, return_exceptions=True)

    def stop_requested(self) -> bool:
        """Check whether the do_run loop should terminate."""
        if self._state_stop_event is not None:
            return self._state_stop_event.is_set()
        return False

    @handle_error
    @debug_log
    def _heartbeat_interrupt(self, reason: str) -> None:
        try:
            self.log_satellite.debug("Attempting to interrupt")
            self._enqueue_transition("interrupt", None, run_in_task=False)
        except Exception:
            pass

    def _mark_degraded(self, reason: str) -> None:
        if self.fsm.state in [SatelliteState.starting, SatelliteState.RUN] and not self.run_degraded:
            self.run_degraded = True
            self.log_satellite.warning("Marking run as degraded: %s", reason)

    # --- satellite state transition wrappers ---

    @handle_error
    @debug_log
    def _wrap_initialize(self, config: Configuration) -> str:
        """Wrapper for the 'initializing' transitional state."""
        if self._state_task is not None and not self._state_task.done():
            if self._state_stop_event:
                self._state_stop_event.set()

        self._pre_initializing_hook(config)
        status_msg: str | None = self.do_initializing(config)
        if not isinstance(status_msg, str):
            status_msg = "Initialized"

        unused_keys = self._store_config(config)
        if unused_keys > 0:
            status_msg += f" ({unused_keys} unused keys)"

        return status_msg

    @debug_log
    def _pre_initializing_hook(self, config: Configuration) -> None:
        """Hook run before do_initializing() is called."""
        config_autonomy = config.get_section("_autonomy", {})
        self._hb_sender.role = config_autonomy.get_enum(CHPRole, "role", CHPRole.DYNAMIC)
        self._hb_sender.max_heartbeat_interval = config_autonomy.get_int("max_heartbeat_interval", 30, min_val=0)

    @debug_log
    def do_initializing(self, config: Configuration) -> str | None:
        """Device-specific initialization. Override in subclass."""
        return "Initialized"

    @handle_error
    @debug_log
    def _wrap_launch(self, payload: Any) -> str:
        """Wrapper for the 'launching' transitional state."""
        self._pre_launching_hook()
        msg: str | None = self.do_launching()
        if not isinstance(msg, str):
            msg = "Launched"
        return msg

    @debug_log
    def _pre_launching_hook(self) -> None:
        """Hook run before do_launching() is called."""
        pass

    @debug_log
    def do_launching(self) -> str | None:
        """Prepare Satellite for data acquisitions. Override in subclass."""
        return "Launched"

    @handle_error
    @debug_log
    def _wrap_reconfigure(self, partial_config: Configuration) -> str:
        """Wrapper for the 'reconfigure' transitional state."""
        status_msg: str | None = self.do_reconfigure(partial_config)  # type: ignore[attr-defined]
        if not isinstance(status_msg, str):
            status_msg = "Reconfigured"

        unused_keys = self._update_config(partial_config)
        if unused_keys > 0:
            status_msg += f" ({unused_keys} unused keys)"

        return status_msg

    @handle_error
    @debug_log
    def _wrap_land(self, payload: Any) -> str:
        """Wrapper for the 'landing' transitional state."""
        msg: str | None = self.do_landing()
        if not isinstance(msg, str):
            msg = "Landed"
        return msg

    @debug_log
    def do_landing(self) -> str:
        """Return Satellite to Initialized state. Override in subclass."""
        return "Landed"

    @handle_error
    @debug_log
    def _wrap_stop(self, payload: Any) -> str:
        """Wrapper for the 'stopping' transitional state."""
        if self._state_stop_event:
            self._state_stop_event.set()
        if self._state_task is not None and not self._state_task.done():
            # Wait for the RUN task to finish
            loop = asyncio.get_event_loop()
            if loop.is_running():
                # Cannot block here, schedule a coroutine instead
                pass
        self.log_satellite.debug("RUN task finished, continue with STOPPING.")
        res: str = self.do_stopping()
        return res

    @debug_log
    def do_stopping(self) -> str:
        """Stop the data acquisition. Override in subclass."""
        return "Stopped"

    @handle_error
    @debug_log
    def _wrap_start(self, run_identifier: str) -> str:
        """Wrapper for the 'run' state of the FSM."""
        self.run_degraded = False
        self.run_identifier = run_identifier
        self.log_satellite.info(f"Starting run '{run_identifier}'")
        msg: str | None = self.do_starting(run_identifier)
        if not isinstance(msg, str):
            msg = ""
        self.stat("RUN_ID", run_identifier)
        self._pre_run_hook()
        self.fsm.complete(msg)
        msg = self.do_run()
        if not isinstance(msg, str):
            msg = ""
        return msg

    @debug_log
    def _pre_run_hook(self) -> None:
        """Hook run immediately before do_run() is called."""
        pass

    @debug_log
    def do_starting(self, run_identifier: str) -> str | None:
        """Final preparation for acquisition. Override in subclass."""
        return "Started"

    @debug_log
    def do_run(self) -> str | None:
        """The acquisition event loop. Override in subclass.

        This method runs in a thread via asyncio.to_thread. Use
        self.stop_requested() to check whether to terminate.
        """
        while not self.stop_requested():
            time.sleep(0.2)
        return "Finished RUN"

    @debug_log
    def _wrap_failure(self, payload: Any) -> str:
        """Wrapper for the 'ERROR' state."""
        try:
            if self._state_stop_event:
                self._state_stop_event.set()
            if self._state_task and not self._state_task.done():
                self._state_task.cancel()
            res: str = self.fail_gracefully()
            return res
        except Exception as e:
            self.log_satellite.exception(e)
            return "Exception caught during failure handling, see logs for details."

    @debug_log
    def fail_gracefully(self) -> str:
        """Method called when reaching 'ERROR' state. Override in subclass."""
        return "Failed gracefully."

    @handle_error
    @debug_log
    def _wrap_interrupt(self, payload: Any) -> str:
        """Wrapper for the 'interrupting' transitional state."""
        res_run: str = ""
        if self._state_stop_event:
            self._state_stop_event.set()
        if self._state_task is not None and not self._state_task.done():
            pass
        self.log_satellite.debug("RUN task finished, continue with INTERRUPTING.")
        res: str = self.do_interrupting()
        return f"{res_run}; {res}"

    @debug_log
    def do_interrupting(self) -> str:
        """Interrupt data acquisition and move to Safe state."""
        self.do_stopping()
        self.do_landing()
        return "Interrupted"

    def _store_config(self, config: Configuration) -> int:
        unused_keys = config._remove_unused_entries()
        if unused_keys:
            self.log.warning(f"{len(unused_keys)} keys of the configuration were not used: {unused_keys}")

        self._config = config

        self.log.info(f"Configuration:{self._config.to_string(ConfigurationGroup.USER)}")
        self.log.debug(f"Configuration:{self._config.to_string(ConfigurationGroup.INTERNAL)}")

        return len(unused_keys)

    def _update_config(self, partial_config: Configuration) -> int:
        unused_keys = partial_config._remove_unused_entries()
        if unused_keys:
            raise RuntimeError(f"{len(unused_keys)} keys of the configuration were not used: {unused_keys}")

        self._config._update(partial_config)

        self.log.info(f"Configuration:{self._config.to_string(ConfigurationGroup.USER)}")
        self.log.debug(f"Configuration:{self._config.to_string(ConfigurationGroup.INTERNAL)}")

        return len(unused_keys)

    # --- device methods ---

    @cscp_requestable()
    def get_version(self) -> tuple[str, Any, dict[str, Any]]:
        """Get Constellation version."""
        return __version__, None, {}

    @cscp_requestable()
    def get_run_id(self) -> tuple[str, Any, dict[str, Any]]:
        """Get current/last known run identifier."""
        return self.run_identifier, None, {}

    @cscp_requestable()
    def get_config(self) -> tuple[str, Any, dict[str, Any]]:
        """Get current satellite configuration."""
        cfg_dict = self._config._dictionary
        return "Dictionary attached in payload", cfg_dict, {}

    @cscp_requestable([SatelliteState.NEW, SatelliteState.INIT, SatelliteState.SAFE, SatelliteState.ERROR])
    def shutdown(self) -> tuple[str, Any, dict[str, Any]]:
        """Queue the Satellite's reentry."""
        self.log_satellite.status("Satellite on reentry course for self-destruction")

        try:
            if self.fsm.state == SatelliteState.RUN:
                self._enqueue_transition("stop", None, run_in_task=False)
                self._enqueue_transition("land", None, run_in_task=False)
            if self.fsm.state == SatelliteState.ORBIT:
                self._enqueue_transition("land", None, run_in_task=False)
        except Exception:
            pass

        # Signal the run loop to stop
        self._async_task_queue.put_nowait(None)

        return f"{self.name} queued for reentry", None, {}


class SatelliteArgumentParser(ConstellationArgumentParser):
    """Customized Argument parser providing common Satellite options."""

    def __init__(self, *args: Any, **kwargs: Any):
        super().__init__(*args, **kwargs)
        self.network.add_argument(
            "--cmd-port",
            "--command-port",
            "--cscp",
            type=int,
            help="The port to listen on for commands sent via the "
            "Constellation Satellite Control Protocol. "
            "A random port will be selected if none is specified.",
        )
        self.network.add_argument(
            "--mon-port",
            "--monitoring-port",
            "--cmdp",
            type=int,
            help="The port to provide data via the "
            "Constellation Monitoring Distribution Protocol. "
            "A random port will be selected if none is specified.",
        )
        self.network.add_argument(
            "--hb-port",
            "--heartbeat-port",
            "--chp",
            type=int,
            help="The port for sending heartbeats via the "
            "Constellation Heartbeat Protocol. "
            "A random port will be selected if none is specified.",
        )
