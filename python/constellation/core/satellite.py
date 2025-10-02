"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2

This module provides the class for a Constellation Satellite.
"""

import threading
import time
import traceback
from concurrent.futures import Future
from queue import Empty
from typing import Any

from . import __version__
from .base import ConstellationArgumentParser
from .chirp import CHIRPServiceIdentifier
from .chirpmanager import CHIRPManager, DiscoveredService, chirp_callback
from .chp import CHPRole
from .commandmanager import CommandReceiver, cscp_requestable
from .configuration import ConfigError, Configuration
from .error import debug_log, handle_error
from .heartbeatchecker import HeartbeatChecker
from .heartbeater import HeartbeatSender
from .message.cscp1 import CSCP1Message, SatelliteState
from .monitoring import MonitoringSender


class Satellite(
    MonitoringSender,
    CommandReceiver,
    CHIRPManager,
    HeartbeatSender,
    HeartbeatChecker,
):
    """Base class for a Constellation Satellite."""

    def __init__(
        self,
        name: str,
        group: str,
        cmd_port: int,
        hb_port: int,
        mon_port: int,
        interface: list[str] | None,
    ):
        """Set up class attributes."""
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
        self.config = Configuration({})

        # give monitoring a chance to start up and catch early messages
        time.sleep(0.1)

        # set up background communication threads
        super()._add_com_thread()
        super()._start_com_threads()

        # register CHIRP services on offer and request heartbeats
        self.register_offer(CHIRPServiceIdentifier.CONTROL, self.cmd_port)
        self.register_offer(CHIRPServiceIdentifier.HEARTBEAT, self.hb_port)
        self.register_offer(CHIRPServiceIdentifier.MONITORING, self.mon_port)
        self.emit_offers()
        self.request(CHIRPServiceIdentifier.HEARTBEAT)

        # Check whether the Satellite has a reconfigure state implemented.
        # If so, add the command to the list of available commands.
        if hasattr(self, "do_reconfigure"):
            self.add_cscp_command("reconfigure")

        # Add exception handling via threading.excepthook to allow the state
        # machine to reflect exceptions in the communication services threads.
        #
        # NOTE: This approach using a global state hook does not play well e.g.
        # with the default pytest configuration, however (see
        # https://github.com/pytest-dev/pytest/discussions/9193). Without
        # disabling the threadexception plugin, tests of the exception handling
        # will fail with pytest.
        threading.excepthook = self._thread_exception
        # greet
        self.log_satellite.info(f"Satellite {self.name}, version {__version__} ready to launch!")

    @debug_log
    @chirp_callback(CHIRPServiceIdentifier.HEARTBEAT)
    def _add_satellite_heartbeat(self, service: DiscoveredService) -> None:
        """Callback method registering satellite's heartbeat."""
        if service.alive:
            self.log_satellite.debug(f"Registering new host for heartbeats at {service.address}:{service.port}")
            self.register_heartbeat_host(service.host_uuid, "tcp://" + service.address + ":" + str(service.port))
        else:
            self.log_satellite.debug(f"Unregistering host for heartbeats at {service.address}:{service.port}")
            self.unregister_heartbeat_host(service.host_uuid)

    def run_satellite(self) -> None:
        """Main Satellite event loop with task handler-routine.

        This routine sequentially executes tasks queued by the CommandReceiver
        or the CHIRPManager. These tasks come in the form of callbacks to
        e.g. state transitions.

        """
        while self._com_thread_evt and not self._com_thread_evt.is_set():
            # TODO: add check for heartbeatchecker: if any entries in hb.get_failed, trigger action
            try:
                # blocking call but with timeout to prevent deadlocks
                task = self.task_queue.get(block=True, timeout=0.5)
                callback = task[0]
                args = task[1]
                try:
                    callback(*args)
                except Exception as e:
                    # TODO consider whether to go into error state if anything goes wrong here
                    self.log_satellite.exception(
                        "Caught exception handling task '%s' with args '%s': %s",
                        callback,
                        args,
                        repr(e),
                    )
            except Empty:
                # nothing to process
                pass
            except KeyboardInterrupt:
                # break line before logging to avoid broken line due to ctrl+c
                print()
                self.log_satellite.warning("Satellite caught KeyboardInterrupt, shutting down.")
                # time to shut down
                break
            time.sleep(0.01)

    def reentry(self) -> None:
        """Orderly shutdown and destroy the Satellite."""
        # can only exit from certain state, go into ERROR if not the case
        self.log_satellite.info("Satellite on reentry course for self-destruction.")
        if self.fsm.current_state_value not in [
            SatelliteState.NEW,
            SatelliteState.INIT,
            SatelliteState.SAFE,
            SatelliteState.ERROR,
        ]:
            err_msg = "Performing controlled re-entry and self-destruction."
            self.fsm.failure(err_msg)
            self._wrap_failure(err_msg)
        super().reentry()

    @handle_error
    @debug_log
    def _heartbeat_interrupt(self, reason: str) -> None:
        try:
            self.log_satellite.debug("Attempting to interrupt")
            self._transition("interrupt", None, thread=False)
        except Exception:
            # Interrupt not allowed, continue
            pass

    def _mark_degraded(self, reason: str) -> None:
        if self.fsm.current_state_value in [SatelliteState.starting, SatelliteState.RUN] and not self.run_degraded:
            self.run_degraded = True
            self.log_satellite.warning("Marking run as degraded: %s", reason)

    # --------------------------- #
    # ----- satellite commands ----- #
    # --------------------------- #

    @handle_error
    @debug_log
    def _wrap_initialize(self, config: Configuration) -> str:
        """Wrapper for the 'initializing' transitional state of the FSM.

        This method performs the basic Satellite transition before passing
        control to the device-specific public method.

        """
        # Verify that there are no running threads left. If there are and the
        # timeout is exceeded joining them, the raised TimeoutError exception
        # will take us into ERROR state.
        try:
            self._state_thread_evt.set()  # type: ignore[union-attr]
            self._state_thread_fut.result(2)  # type: ignore[union-attr]
        except AttributeError:
            # no threads left
            pass
        self._state_thread_evt = None
        self._state_thread_fut = None

        # Store configuration
        self.config = config

        # call device-specific user-routine
        try:
            self._pre_initializing_hook(self.config)
            init_msg: str | None = self.do_initializing(self.config)
            if not isinstance(init_msg, str):
                init_msg = "Initialized"
            if self.config.has_unused_values():
                for key in self.config.get_unused_keys():
                    self.log_satellite.warning("Satellite ignored configuration value: '%s'", key)
                init_msg += " IGNORED parameters: "
                init_msg += ",".join(self.config.get_unused_keys())
        except ConfigError as e:
            msg = "Caught exception during initialization: "
            msg += f"missing a required configuration value {e}?"
            self.log_satellite.error(msg)
            raise RuntimeError(msg) from e
        return init_msg

    @debug_log
    def _pre_initializing_hook(self, config: Configuration) -> None:
        """Hook run before do_initializing() is called.

        Allows inheriting (core) classes to perform actions immediately before
        user code is executed.

        """
        self.role = CHPRole[self.config.setdefault("_role", "DYNAMIC").upper()]

    @debug_log
    def do_initializing(self, config: Configuration) -> str | None:
        """Method for the device-specific code of 'initializing' transition.

        This should set configuration variables.

        """
        return "Initialized"

    @handle_error
    @debug_log
    def _wrap_launch(self, payload: Any) -> str:
        """Wrapper for the 'launching' transitional state of the FSM.

        This method performs the basic Satellite transition before passing
        control to the device-specific public method.

        """
        msg: str | None = self.do_launching()
        if not isinstance(msg, str):
            msg = "Launched"
        return msg

    @debug_log
    def do_launching(self) -> str | None:
        """Prepare Satellite for data acquisitions."""
        return "Launched"

    @handle_error
    @debug_log
    def _wrap_reconfigure(self, partial_config: Configuration) -> str:
        """Wrapper for the 'reconfigure' transitional state of the FSM.

        This method performs the basic Satellite transition before passing
        control to the device-specific public method.

        """

        # reconfigure is not necessarily implemented; it is not in the this base
        # class to allow checking for the exististance of the method to
        # determine the reaction to a `reconfigure` CSCP command.
        init_msg: str | None = self.do_reconfigure(partial_config)  # type: ignore[attr-defined]
        if not isinstance(init_msg, str):
            init_msg = "Reconfigured"

        # update config
        self.config.update(partial_config.get_dict(), partial_config.get_unused_keys())

        if partial_config.has_unused_values():
            for key in partial_config.get_unused_keys():
                self.log_satellite.warning("Satellite ignored configuration value: '%s'", key)
            init_msg += " IGNORED parameters: "
            init_msg += ",".join(self.config.get_unused_keys())
        return init_msg

    @handle_error
    @debug_log
    def _wrap_land(self, payload: Any) -> str:
        """Wrapper for the 'landing' transitional state of the FSM.

        This method performs the basic Satellite transition before passing
        control to the device-specific public method.

        """
        msg: str | None = self.do_landing()
        if not isinstance(msg, str):
            msg = "Landed"
        return msg

    @debug_log
    def do_landing(self) -> str:
        """Return Satellite to Initialized state."""
        return "Landed"

    @handle_error
    @debug_log
    def _wrap_stop(self, payload: Any) -> str:
        """Wrapper for the 'stopping' transitional state of the FSM.

        This method performs the basic Satellite transition before passing
        control to the device-specific public method.

        """
        # indicate to the current acquisition thread to stop
        if self._state_thread_evt:
            self._state_thread_evt.set()
        # wait for result, waiting until done
        # assert for mypy static type analysis
        assert isinstance(self._state_thread_fut, Future)
        res_run: str = self._state_thread_fut.result(timeout=None)
        self.log_satellite.debug("RUN thread finished, continue with STOPPING.")
        res: str = self.do_stopping()
        return f"{res_run}; {res}"

    @debug_log
    def do_stopping(self) -> str:
        """Stop the data acquisition."""
        return "Stopped"

    @handle_error
    @debug_log
    def _wrap_start(self, run_identifier: str) -> str:
        """Wrapper for the 'run' state of the FSM.

        This method performs the basic Satellite transition before passing
        control to the device-specific public method.

        """
        self.run_degraded = False
        self.run_identifier = run_identifier
        self.log_satellite.info(f"Starting run '{run_identifier}'")
        msg: str | None = self.do_starting(run_identifier)
        if not isinstance(msg, str):
            msg = ""
        # allow inheriting classes to execute code just before do_run is called:
        self._pre_run_hook(run_identifier)
        # complete transitional state
        self.fsm.complete(msg)
        # continue to execute DAQ in this thread
        msg = self.do_run(run_identifier)
        if not isinstance(msg, str):
            msg = ""
        return msg

    @debug_log
    def _pre_run_hook(self, run_identifier: str) -> None:
        """Hook run immediately before do_run() is called.

        Intended for inheriting classes to inject code in between calls to
        do_starting() and do_run().

        """
        pass

    @debug_log
    def do_starting(self, run_identifier: str) -> str | None:
        """Final preparation for acquisition."""
        return "Started"

    @debug_log
    def do_run(self, run_identifier: str) -> str | None:
        """The acquisition event loop.

        This method will be started by the Satellite and run in a thread. It
        therefore needs to monitor the self.stop_running Event and close itself
        down if the Event is set.

        NOTE: This method is not inherently thread-safe as it runs in the
        context of the Satellite and can modify data accessible to the main
        thread. However, the state machine can effectively act as a lock and
        prevent competing access to the same objects while in RUNNING state as
        long as care is taken in the implementation.

        The state machine itself uses the RTC model by default (see
        https://python-statemachine.readthedocs.io/en/latest/processing_model.html?highlight=thread)
        which should make the transitions themselves safe.

        """
        # the stop_running Event will be set from outside the thread when it is
        # time to close down.
        # assert for mypy static type analysis
        assert isinstance(self._state_thread_evt, threading.Event), "Transition thread Event not set up correctly"
        while not self._state_thread_evt.is_set():
            time.sleep(0.2)
        return "Finished RUN"

    @debug_log
    def _wrap_failure(self, payload: Any) -> str:
        """Wrapper for the 'ERROR' state of the FSM.

        This method performs the basic Satellite transition before passing
        control to the device-specific public method.

        """
        try:
            # stop state thread
            if self._state_thread_evt:
                self._state_thread_evt.set()
                if self._state_thread_fut:
                    try:
                        self._state_thread_fut.result(timeout=1)
                    except TimeoutError:
                        self.log_satellite.error("Timeout while joining state thread, continuing.")
            res: str = self.fail_gracefully()
            return res
        # NOTE: we cannot have a non-handled exception disallow the state
        # transition to failure state!
        except Exception as e:
            self.log_satellite.exception(e)
            return "Exception caught during failure handling, see logs for details."

    @debug_log
    def fail_gracefully(self) -> str:
        """Method called when reaching 'ERROR' state."""
        return "Failed gracefully."

    @handle_error
    @debug_log
    def _wrap_interrupt(self, payload: Any) -> str:
        """Wrapper for the 'interrupting' transitional state of the FSM.

        This method performs the basic Satellite transition before passing
        control to the device-specific public method.

        """
        # indicate to the current acquisition thread to stop
        res_run: str = ""
        if self._state_thread_evt:
            self._state_thread_evt.set()
            # wait for result, will block until user code finishes
            # assert for mypy static type analysis
            assert isinstance(self._state_thread_fut, Future)
            res_run = self._state_thread_fut.result(timeout=None)
        self.log_satellite.debug("RUN thread finished, continue with INTERRUPTING.")
        res: str = self.do_interrupting()
        return f"{res_run}; {res}"

    @debug_log
    def do_interrupting(self) -> str:
        """Interrupt data acquisition and move to Safe state.

        Defaults to calling the stop and land handlers.
        """
        self.do_stopping()
        self.do_landing()
        return "Interrupted"

    def _thread_exception(self, args: Any) -> None:
        """Handle exceptions in threads.

        Change state to FAULT.

        Intended to be installed as threading.excepthook.

        """
        tb = (
            "Traceback (most recent call last):\n"
            + "".join(traceback.format_tb(args.exc_traceback))
            + f"{args.exc_type.__name__}: {args.exc_value}"
        )
        self.log_satellite.critical(
            f"{args.exc_type.__name__} in {args.thread._name}: {args.exc_value}",
            extra={"traceback": tb},
        )
        # change internal state
        err_msg = f"Thread {args.thread} failed. Caught exception {args.exc_type} with value {args.exc_value}."
        self.fsm.failure(err_msg)
        self._wrap_failure(err_msg)

    # -------------------------- #
    # ----- device methods ----- #
    # -------------------------- #

    @cscp_requestable
    def get_version(self, _request: CSCP1Message | None = None) -> tuple[str, Any, dict[str, Any]]:
        """Get Constellation version.

        No payload argument.

        Additional version information may be included in the meta map (final
        return value).

        """
        return __version__, None, {}

    @cscp_requestable
    def get_run_id(self, _request: CSCP1Message | None = None) -> tuple[str, Any, dict[str, Any]]:
        """Get current/last known run identifier.

        No payload argument.

        """
        return self.run_identifier, None, {}

    @cscp_requestable
    def get_config(self, _request: CSCP1Message | None = None) -> tuple[str, Any, dict[str, Any]]:
        """Get current satellite configuration.

        No payload argument.

        """
        cfg_dict = self.config.get_dict()
        return f"{len(cfg_dict)} configuration keys, dictionary attached in payload", cfg_dict, {}


# -------------------------------------------------------------------------


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
