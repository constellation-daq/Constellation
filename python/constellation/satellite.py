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

from .fsm import SatelliteStateHandler
from . import __version__
from .heartbeater import Heartbeater
from .heartbeatchecker import HeartbeatChecker

from .cscp import CSCPMessage
from .chirp import CHIRPServiceIdentifier
from .broadcastmanager import CHIRPBroadcaster
from .commandmanager import CommandReceiver, cscp_requestable
from .log_and_stats import getLoggerAndStats
from .error import debug_log, handle_error


class IncompleteCommand(Exception):
    pass


class Satellite(CommandReceiver, CHIRPBroadcaster, SatelliteStateHandler):
    """Base class for a Constellation Satellite."""

    def __init__(
        self, name: str, group: str, cmd_port: int, hb_port: int, log_port: int
    ):
        """Set up class attributes."""
        super().__init__(
            name=name,
            group=group,
            cmd_port=cmd_port,
            hb_port=hb_port,
            log_port=log_port,
        )

        # set up python logging
        self.log, self.stats = getLoggerAndStats(self.name, self.context, log_port)

        # set up background communication threads
        # NOTE should be a late part of the initialization, as it starts communication
        super()._add_com_thread()
        super()._start_com_threads()

        # register and start heartbeater
        self.heartbeater = Heartbeater(
            self.get_state, f"tcp://*:{hb_port}", context=self.context
        )
        self.heartbeater.start()

        # register heartbeat checker
        self.hb_checker = HeartbeatChecker()

        # register broadcast manager
        self.register_offer(CHIRPServiceIdentifier.CONTROL, cmd_port)
        self.register_offer(CHIRPServiceIdentifier.HEARTBEAT, hb_port)
        self.register_offer(CHIRPServiceIdentifier.MONITORING, log_port)
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
        """Main event loop with task handler-routine"""
        while True:
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

            time.sleep(0.05)
        # TODO add a 'finally:' which closes zmq context via .term() and cleans
        # up other things OR setup an atexit hook
        #
        # on exit: stop heartbeater
        self.heartbeater.stop()
        # TODO : shutdown broadcast manager and depart

    # --------------------------- #
    # ----- satellite commands ----- #
    # --------------------------- #

    @handle_error
    @debug_log
    def on_initialize(self, payload: any):
        """Callback method for the 'initialize' transition of the FSM.

        Set and check config, maybe initialize device.
        """
        # TODO on_initialize should (re-)load config values
        #
        # Verify that there are no running threads left. If there are and the
        # timeout is exceeded joining them, the raised exception will take us
        # into ERROR state.

    @handle_error
    def on_launch(self, payload: any):
        """Prepare Satellite for data acquistions."""
        self.hb_checker.start()

    @handle_error
    def on_land(self, payload: any):
        """Land transition of the FSM: Return Satellite to Initialized state."""
        self.hb_checker.stop()

    @handle_error
    def on_start(self, payload: any):
        """Method for the 'start' transition of the FSM."""
        self.do_run(payload)
        return "Acquisition finished."

    @handle_error
    def on_stop(self, payload: any):
        """Method for the 'stop' transition of the FSM."""
        # indicate to the thread to stop
        if self._state_thread_evt:
            self._state_thread_evt.set()
        # wait for result, will raise TimeoutError if not successful
        self._state_thread_fut.result(timeout=10)

    def do_run(self, payload: any):
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

    def on_failure(self):
        """Failure transition of the FSM.

        This method should implement Satellite-specific actions in case of
        failure.

        """
        try:
            # Stop heartbeat checking
            self.hb_checker.stop()
            # stop state thread
            if self._state_thread_evt:
                self._state_thread_evt.set()
        # NOTE: we cannot have a non-handled exception disallow the state
        # transition to failure state!
        except Exception as e:
            self.log.exception(e)

    @handle_error
    def on_interrupt(self):
        """Interrupt data acquisition and move to Safe state.

        Defaults to calling on_failure().
        """
        self.on_failure()

    @handle_error
    def on_recover(self):
        """Transition to Initialized state.

        Defaults to on_initialize().
        """
        self.on_initialize()

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
        # indicate to remaining thread to stop
        if self._stop_running:
            self._stop_running.set()
        # change internal state
        self.fsm.failure(
            f"Thread {args.thread} failed. Caught exception {args.exc_type} \
            with value {args.exc_value}."
        )

    # -------------------------- #
    # ----- device methods ----- #
    # -------------------------- #

    @cscp_requestable
    def get_state(self, _request: CSCPMessage = None) -> str:
        return self.fsm.current_state.id, None, None

    @cscp_requestable
    def get_status(self, _request: CSCPMessage = None) -> str:
        return self.fsm.status, None, None

    @cscp_requestable
    def version(self, _request: CSCPMessage = None):
        """Get Constellation version."""
        return __version__, None, None


# -------------------------------------------------------------------------


def main(args=None):
    """Start the base Satellite server."""
    import argparse

    parser = argparse.ArgumentParser(description=main.__doc__)
    parser.add_argument("--log-level", default="info")
    parser.add_argument("--cmd-port", type=int, default=23999)
    parser.add_argument("--log-port", type=int, default=5556)  # Should be 55556?
    parser.add_argument("--hb-port", type=int, default=61234)
    parser.add_argument("--name", type=str, default="satellite_demo")
    parser.add_argument("--group", type=str, default="constellation")
    args = parser.parse_args(args)

    # set up logging
    logger = logging.getLogger()  # get root logger
    formatter = logging.Formatter(
        "%(asctime)s | %(name)s |  %(levelname)s: %(message)s"
    )
    # global level should be the lowest level that we want to see on any
    # handler, even streamed via ZMQ
    logger.setLevel(0)

    stream_handler = logging.StreamHandler()
    stream_handler.setLevel(args.log_level.upper())
    stream_handler.setFormatter(formatter)
    logger.addHandler(stream_handler)

    logger.info("Starting up satellite!")
    # start server with remaining args
    s = Satellite(args.name, args.group, args.cmd_port, args.hb_port, args.log_port)
    s.run_satellite()


if __name__ == "__main__":
    main()
