"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0

This module provides a base class for Constellation Satellite modules.
"""

import zmq
import threading
import logging
from queue import Queue
import atexit


SATELLITE_LIST = []


@atexit.register
def destroy_satellites():
    """Close down connections and perform orderly re-entry."""
    for sat in SATELLITE_LIST:
        sat.reentry()


class BaseSatelliteFrame:
    """Base class for all Satellite components to inherit from.

    Provides the basic internal Satellite infrastructure related to logging, ZMQ
    and threading that all Satellite communication service components (i.e.
    mixin classes) share.

    """

    def __init__(self, name, **_kwds):
        # add class name to create the canonical name
        self.name = f"{type(self).__name__}.{name}"
        self.log = logging.getLogger(name)
        self.context = zmq.Context()

        # Set up a queue for handling tasks related to incoming requests via
        # CSCP or offers via CHIRP. This makes sure that these can be performed
        # thread-safe from the main thread of the Satellite. A 'task' consists
        # of a tuple consisting of a callback method and an object passed as
        # argument (DiscoveredService and CSCPMessage for CHIRP and CSCP,
        # respectively).
        self.task_queue = Queue()

        # dict to keep references to all communication service threads usually
        # running in the background
        self._com_thread_pool: dict(str, threading.Thread) = {}
        # Event to indicate to communication service threads to stop
        self._com_thread_evt: threading.Event = None

        # add self to list of satellites to destroy on shutdown
        global SATELLITE_LIST
        SATELLITE_LIST.append(self)

    def _add_com_thread(self):
        """Method to add a background communication service thread to the pool.

        Does nothing in the base class.

        """
        self.log.debug("Satellite Base class _add_thread called")
        pass

    def _start_com_threads(self):
        """Start all background communication threads."""
        self._com_thread_evt = threading.Event()
        for component, thread in self._com_thread_pool.items():
            self.log.debug("Starting thread for %s communication", component)
            thread.start()

    def _stop_com_threads(self, timeout: int = 1.5):
        """Stop all background communication threads within timeout [s]."""
        if self._com_thread_evt:
            self._com_thread_evt.set()
            for component, thread in self._com_thread_pool.items():
                if thread.is_alive():
                    thread.join(timeout)
                    # check if thread is still alive
                    if thread.is_alive():
                        self.log.error(
                            "Could not join background communication thread for %s within timeout",
                            component,
                        )
                        raise RuntimeError(
                            f"Could not join running thread for {component} within timeout of {timeout}s!"
                        )
        self._com_thread_evt = None
        self._com_thread_pool = {}

    def reentry(self):
        """Orderly destroy the satellite."""
        self.log.debug("Stopping all communication threads.")
        self._stop_com_threads()
        self.log.debug("Terminating ZMQ context.")
        self.context.term()