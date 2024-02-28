#!/usr/bin/env python3
"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0
"""

from enum import Enum, auto
from statemachine import StateMachine
from statemachine.states import States


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









