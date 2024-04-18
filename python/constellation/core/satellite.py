#!/usr/bin/env python3
"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0

This module provides the class for a Constellation Satellite.
"""

import time
from queue import Empty
import logging
import threading
import traceback

from .fsm import SatelliteState
from . import __version__
from .heartbeater import HeartbeatSender
from .heartbeatchecker import HeartbeatChecker

from .cscp import CSCPMessage
from .chirp import CHIRPServiceIdentifier
from .broadcastmanager import CHIRPBroadcaster
from .commandmanager import CommandReceiver, cscp_requestable
from .confighandler import Configuration
from .monitoring import MonitoringSender
from .error import debug_log, handle_error


class Satellite(
    CommandReceiver,
    CHIRPBroadcaster,
    MonitoringSender,
    HeartbeatSender,
):
    """Base class for a Constellation Satellite."""

    def __init__(
        self,
        name: str,
        group: str,
        cmd_port: int,
        hb_port: int,
        mon_port: int,
        interface: str,
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

        # give monitoring a chance to start up and catch early messages
        time.sleep(0.1)

        # set up background communication threads
        super()._add_com_thread()
        super()._start_com_threads()

        # register heartbeat checker
        self.hb_checker = HeartbeatChecker()

        # register broadcast manager
        self.register_offer(CHIRPServiceIdentifier.CONTROL, cmd_port)
        self.register_offer(CHIRPServiceIdentifier.HEARTBEAT, hb_port)
        self.register_offer(CHIRPServiceIdentifier.MONITORING, mon_port)
        self.broadcast_offers()

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
        self.log.info(f"Satellite {self.name}, version {__version__} ready to launch!")

    @debug_log
    @cscp_requestable
    def register(self, request: CSCPMessage):
        """Register a heartbeat via CSCP request."""
        name, ip, port = request.payload.split()
        callback = self.hb_checker.register
        # add to the task queue
        self.task_queue.put((callback, [name, f"tcp://{ip}:{port}", self.context]))
        return "registering", name, None

    def run_satellite(self):
        """Main Satellite event loop with task handler-routine.

        This routine sequenctially executes tasks queued by the CommandReceiver
        or the CHIRPBroadcaster. These tasks come in the form of callbacks to
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
                    self.log.error("Caught exception handling task: %s", repr(e))
            except Empty:
                # nothing to process
                pass
            except KeyboardInterrupt:
                self.log.warning("Satellite caught KeyboardInterrupt, shutting down.")
                # time to shut down
                break
            time.sleep(0.01)

    def reentry(self):
        """Orderly shutdown and destroy the Satellelite."""
        # can only exit from certain state, go into ERROR if not the case
        self.log.info("Satellite on reentry course for self-destruction.")
        if self.fsm.current_state.id not in [
            SatelliteState.NEW,
            SatelliteState.INIT,
            SatelliteState.SAFE,
            SatelliteState.ERROR,
        ]:
            self.fsm.failure("Performing controlled re-entry and self-destruction.")
            self._wrap_failure()
        super().reentry()

    # --------------------------- #
    # ----- satellite commands ----- #
    # --------------------------- #

    @handle_error
    @debug_log
    def _wrap_initialize(self, payload: any) -> str:
        """Wrapper for the 'initializing' transitional state of the FSM.

        This method performs the basic Satellite transition before passing
        control to the device-specific public method.

        """
        # Verify that there are no running threads left. If there are and the
        # timeout is exceeded joining them, the raised TimeoutError exception
        # will take us into ERROR state.
        try:
            self._state_thread_evt.set()
            self._state_thread_fut.result(2)
        except AttributeError:
            # no threads left
            pass
        self._state_thread_evt = None
        self._state_thread_fut = None

        self.config = Configuration(payload)
        init_msg = self.do_initializing(payload)

        if self.config.has_unused_values():
            for key in self.config.get_unused_keys():
                self.log.warning("Satellite ignored configuration value: '%s'", key)
        return init_msg

    @debug_log
    def do_initializing(self, payload: any) -> str:
        """Method for the device-specific code of 'initializing' transition.

        This should set configuration variables.

        """
        return "Initialized."

    @handle_error
    @debug_log
    def _wrap_launch(self, payload: any) -> str:
        """Wrapper for the 'launching' transitional state of the FSM.

        This method performs the basic Satellite transition before passing
        control to the device-specific public method.

        """
        self.hb_checker.start()
        return self.do_launching(payload)

    @debug_log
    def do_launching(self, payload: any) -> str:
        """Prepare Satellite for data acquistions."""
        return "Launched."

    @handle_error
    @debug_log
    def _wrap_land(self, payload: any) -> str:
        """Wrapper for the 'landing' transitional state of the FSM.

        This method performs the basic Satellite transition before passing
        control to the device-specific public method.

        """
        self.hb_checker.stop()
        return self.do_landing(payload)

    @debug_log
    def do_landing(self, payload: any) -> str:
        """Return Satellite to Initialized state."""
        return "Landed."

    @handle_error
    @debug_log
    def _wrap_stop(self, payload: any):
        """Wrapper for the 'stopping' transitional state of the FSM.

        This method performs the basic Satellite transition before passing
        control to the device-specific public method.

        """
        # indicate to the current acquisition thread to stop
        if self._state_thread_evt:
            self._state_thread_evt.set()
        # wait for result, will raise TimeoutError if not successful
        self._state_thread_fut.result(timeout=10)
        self._state_thread_evt = None
        return self.do_stopping(payload)

    @debug_log
    def do_stopping(self, payload: any):
        """Stop the data acquisition."""
        return "Acquisition stopped."

    @handle_error
    @debug_log
    def _wrap_start(self, payload: any) -> str:
        """Wrapper for the 'run' state of the FSM.

        This method performs the basic Satellite transition before passing
        control to the device-specific public method.

        """
        res = self.do_starting(payload)
        # complete transitional state
        self.fsm.complete(res)
        # continue to execute DAQ in this thread
        return self.do_run(payload)

    @debug_log
    def do_starting(self, payload: any) -> str:
        """Final preparation for acquisition."""
        return "Finished preparations, starting."

    @debug_log
    def do_run(self, payload: any) -> str:
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
        while not self._state_thread_evt.is_set():
            time.sleep(0.2)
        return "Finished acquisition."

    @debug_log
    def _wrap_failure(self):
        """Wrapper for the 'ERROR' state of the FSM.

        This method performs the basic Satellite transition before passing
        control to the device-specific public method.

        """
        try:
            # Stop heartbeat checking
            self.hb_checker.stop()
            # stop state thread
            if self._state_thread_evt:
                self._state_thread_evt.set()
                self._state_thread.join(1)
            return self.fail_gracefully()
        # NOTE: we cannot have a non-handled exception disallow the state
        # transition to failure state!
        except Exception as e:
            self.log.exception(e)
            return "Exception caught during failure handling, see logs for details."

    @debug_log
    def fail_gracefully(self):
        """Method called when reaching 'ERROR' state."""
        return "Failed gracefully."

    @handle_error
    @debug_log
    def _wrap_interrupt(self, payload):
        """Wrapper for the 'interrupting' transitional state of the FSM.

        This method performs the basic Satellite transition before passing
        control to the device-specific public method.

        """
        # indicate to the current acquisition thread to stop
        if self._state_thread_evt:
            self._state_thread_evt.set()
            # wait for result, will raise TimeoutError if not successful
            self._state_thread_fut.result(timeout=10)
            self._state_thread_evt = None
        self.hb_checker.stop()
        return self.do_interrupting()

    @debug_log
    def do_interrupting(self):
        """Interrupt data acquisition and move to Safe state.

        Defaults to calling the stop and land handlers.
        """
        self.do_stopping()
        self.do_landing()
        return "Interrupted."

    @handle_error
    @debug_log
    def _wrap_recover(self):
        """Wrapper for the 'recovering' transitional state of the FSM.

        This method does not perform any action as the SAFE->INIT transition has
        no side-effects.

        """
        return "Recovered."

    def _thread_exception(self, args):
        """Handle exceptions in threads.

        Change state to FAULT.

        Intended to be installed as threading.excepthook.

        """
        tb = "".join(traceback.format_tb(args.exc_traceback))
        self.log.fatal(
            f"caught {args.exc_type} with value \
            {args.exc_value} in thread {args.thread} and traceback {tb}."
        )
        self._wrap_failure()
        # change internal state
        self.fsm.failure(
            f"Thread {args.thread} failed. Caught exception {args.exc_type} \
            with value {args.exc_value}."
        )

    # -------------------------- #
    # ----- device methods ----- #
    # -------------------------- #

    @cscp_requestable
    def get_version(self, _request: CSCPMessage = None) -> (str, None, None):
        """Get Constellation version.

        No payload argument.

        Additional version information may be included in the meta map (final
        return value).

        """
        return __version__, None, None


# -------------------------------------------------------------------------


def main(args=None):
    """Start the base Satellite server."""
    import argparse
    import coloredlogs

    parser = argparse.ArgumentParser(description=main.__doc__)
    parser.add_argument("--log-level", default="info")
    parser.add_argument("--cmd-port", type=int, default=23999)
    parser.add_argument("--mon-port", type=int, default=55556)
    parser.add_argument("--hb-port", type=int, default=61234)
    parser.add_argument("--interface", type=str, default="*")
    parser.add_argument("--name", type=str, default="satellite_demo")
    parser.add_argument("--group", type=str, default="constellation")
    args = parser.parse_args(args)

    # set up logging
    logger = logging.getLogger(args.name)
    coloredlogs.install(level=args.log_level.upper(), logger=logger)

    logger.info("Starting up satellite!")
    # start server with remaining args
    s = Satellite(
        name=args.name,
        group=args.group,
        cmd_port=args.cmd_port,
        hb_port=args.hb_port,
        mon_port=args.mon_port,
        interface=args.interface,
    )
    s.run_satellite()


if __name__ == "__main__":
    main()