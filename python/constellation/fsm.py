#!/usr/bin/env python3
"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0
"""

from threading import Event
from concurrent.futures import ThreadPoolExecutor, Future
from enum import Enum, auto
from statemachine import StateMachine, TransitionNotAllowed
from statemachine.states import States

from .cscp import CSCPMessage
from .error import debug_log, handle_error
from .base import SatelliteBaseFrame
from .commandmanager import cscp_requestable


class SatelliteState(Enum):
    """Available states to cycle through."""

    # Idle state without any configuration
    NEW = auto()
    # Initialized state with configuration but not (fully) applied
    INIT = auto()
    # Prepared state where configuration is applied
    ORBIT = auto()
    # Running state where DAQ is running
    RUN = auto()
    # Safe fallback state if error is discovered during run
    SAFE = auto()
    # Error state if something went wrong
    ERROR = auto()
    #
    #  TRANSITIONAL STATES
    #
    initializing = auto()
    launching = auto()
    landing = auto()
    reconfiguring = auto()
    starting = auto()
    stopping = auto()
    interrupting = auto()


class SatelliteFSM(StateMachine):
    """Manage the satellite's state and its transitions."""

    # Convert enum to states
    states = States.from_enum(SatelliteState, initial=SatelliteState.NEW)

    # Define transitions
    # - NEW <=> INIT
    initialize = states.NEW.to(states.initializing) | states.INIT.to(
        states.initializing
    )
    initialize |= states.ERROR.to(states.initializing)

    initialized = states.initializing.to(states.INIT)

    # - INIT <=> ORBIT
    launch = states.INIT.to(states.launching)
    launched = states.launching.to(states.ORBIT)

    land = states.ORBIT.to(states.landing)
    landed = states.landing.to(states.INIT)

    # - ORBIT <=> RUN
    start = states.ORBIT.to(states.starting)
    started = states.starting.to(states.RUN)

    stop = states.RUN.to(states.stopping)
    stopped = states.stopping.to(states.ORBIT)

    reconfigure = states.ORBIT.to(states.reconfiguring)
    reconfigured = states.reconfiguring.to(states.ORBIT)

    # - safe
    interrupt = states.ORBIT.to(states.interrupting) | states.RUN.to(
        states.interrupting
    )
    interrupted = states.interrupting.to(states.SAFE)

    recover = states.SAFE.to(states.INIT)
    # - error
    failure = states.RUN.to(states.ERROR) | states.ORBIT.to(states.ERROR)
    failure |= states.INIT.to(states.ERROR) | states.SAFE.to(states.ERROR)
    failure |= states.ERROR.to(states.ERROR) | states.NEW.to(states.ERROR)

    failure |= states.initializing.to(states.ERROR)
    failure |= states.launching.to(states.ERROR)
    failure |= states.landing.to(states.ERROR)
    failure |= states.reconfiguring.to(states.ERROR)
    failure |= states.starting.to(states.ERROR)
    failure |= states.stopping.to(states.ERROR)
    failure |= states.interrupting.to(states.ERROR)

    # complete a transitional state
    complete = initialized | launched | landed | started
    complete |= stopped | reconfigured | interrupted

    def __init__(self):
        self.status = "Satellite not initialized yet."
        super().__init__()

    def before_transition(self, status):
        """Set status before the state change."""
        self.status = status

    def write_diagram(self, filename):
        """Create a png with the FSM schematic."""
        from statemachine.contrib.diagram import DotGraphMachine

        graph = DotGraphMachine(self)
        dot = graph()
        dot.write_png(filename)


class SatelliteStateHandler(SatelliteBaseFrame):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

        # instantiate state machine
        self.fsm = SatelliteFSM()

        # (transitional) state executor and event
        self._state_thread_evt: Event = None
        self._state_thread_exc = ThreadPoolExecutor(max_workers=1)
        self._state_thread_fut: Future = None

    @debug_log
    @cscp_requestable
    def initialize(self, request: CSCPMessage):
        """Queue a state transition via a CSCP request.

        If the transition is not allowed, TransitionNotAllowed will be thrown by
        the FSM.

        """
        self._transition("initialize", request, thread=False)

    @debug_log
    @cscp_requestable
    def launch(self, request: CSCPMessage):
        """Queue a state transition via a CSCP request.

        If the transition is not allowed, TransitionNotAllowed will be thrown by
        the FSM.

        """
        self._transition("launch", request, thread=False)

    @debug_log
    @cscp_requestable
    def land(self, request: CSCPMessage):
        """Queue a state transition via a CSCP request.

        If the transition is not allowed, TransitionNotAllowed will be thrown by
        the FSM.

        """
        self._transition("land", request, thread=False)

    @debug_log
    @cscp_requestable
    def start(self, request: CSCPMessage):
        """Queue a state transition via a CSCP request.

        If the transition is not allowed, TransitionNotAllowed will be thrown by
        the FSM.

        """
        self._transition("start", request, thread=True)

    @debug_log
    @cscp_requestable
    def stop(self, request: CSCPMessage):
        """Queue a state transition via a CSCP request.

        If the transition is not allowed, TransitionNotAllowed will be thrown by
        the FSM.

        """
        self._transition("stop", request, thread=False)

    @debug_log
    @cscp_requestable
    def reconfigure(self, request: CSCPMessage):
        """Queue a reconfigure state transition via a CSCP request.

        If the transition is not allowed, TransitionNotAllowed will be thrown by
        the FSM.

        """
        if not hasattr(self, "on_reconfigure"):
            raise NotImplementedError("Reconfigure not supported")
        self._transition("reconfigure", request, thread=False)

    @debug_log
    @cscp_requestable
    def interrupt(self, request: CSCPMessage):
        """Queue a state transition via a CSCP request.

        If the transition is not allowed, TransitionNotAllowed will be thrown by
        the FSM.

        """
        self._transition("interrupt", request, thread=False)

    @debug_log
    @cscp_requestable
    def recover(self, request: CSCPMessage):
        """Queue a state transition via a CSCP request.

        If the transition is not allowed, TransitionNotAllowed will be thrown by
        the FSM.

        """
        self._transition("recover", request, thread=False)

    @debug_log
    @cscp_requestable
    def failure(self, request: CSCPMessage):
        """Queue an error state transition via a CSCP request.

        This is intended for debugging purposes only and should not be called in
        normal operation.

        """
        self._transition("failure", request, thread=False)

    def _transition(self, target: str, request: CSCPMessage, thread: bool):
        """Prepare and enqeue a transition task.

        The task consists of the respective transition method and the request
        payload as argument.

        If thread is true, then the transition method will be run as a separate
        thread. If allowed by the FSM, threads can potentially be interrupted by
        a subsequent task such as 'RUN' can be stopped via the 'stop' command.

        Otherwise, the transition will be run in the main satellite thread and
        subsequent tasks are queued until the original transition has finished.

        """
        # call FSM transition, will throw if not allowed
        getattr(self.fsm, target)(f"{target.capitalize()} called via CSCP request.")
        transit_fcn = getattr(self, f"on_{target}")
        # add to the task queue to run from the main thread
        if thread:
            # task will be run in a separate thread
            self.task_queue.put(
                (self._start_transition_thread, (transit_fcn, request.payload))
            )
        else:
            # task will be executed within the main satellite thread
            self.task_queue.put((transit_fcn, request.payload))
        return "transitioning", target, None

    def _start_transition(self, fcn: callable, payload: any):
        """Start a transition and advance FSM for transitional states."""
        res = fcn(payload)
        if not res:
            res = "Transition completed!"
        # try to advance the FSM for finishing transitional states
        try:
            self.fsm.complete(res)
        except TransitionNotAllowed:
            # no need to do more than set the status, we are in a steady
            # operational state
            self.fsm.status = res

    def _start_transition_thread(self, fcn: callable, payload: any):
        """Start a transition thread with the given fcn and arguments."""
        self._state_thread_evt = Event()
        self._state_thread_fut = self._state_thread_exc.submit(fcn, payload)
        # add a callback when transition is complete
        self._state_thread_fut.add_done_callback(self._state_transition_thread_complete)

    @handle_error
    def _state_transition_thread_complete(self, fut: Future):
        """Callback method when a transition thread is done."""
        self.log.debug("Transition completed and callback received.")
        # Get the thread's return value. This raises any exception thrown in the
        # thread, which will be handled by the @handle_error decorator to put us
        # into ERROR state.
        # TODO: test that this is the case
        res = fut.result()
        if not res:
            res = "Transition completed!"
        # try to advance the FSM for finishing transitional states
        try:
            self.fsm.complete(res)
        except TransitionNotAllowed:
            # no need to do more than set the status, we are in a steady
            # operational state
            self.fsm.status = res
        # cleanup
        self._state_thread_evt = None
