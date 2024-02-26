"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0

This module provides a base class for Constellation Satellite modules.
"""

import zmq
import threading
import logging


class BaseSatelliteFrame:
    """Base class for all Satellite components to inherit from.

    Provides the basic internal Satellite infrastructure related to logging, ZMQ
    and threading that all Satellite components (i.e. mixin classes) share.

    """

    def __init__(self, name):
        self.name = name
        self.log = logging.getLogger(name)
        self.context = zmq.Context()

        # dict to keep references to all communication threads usually running
        # in the background
        self._com_thread_pool: dict(str, threading.Thread) = {}
        # Event to indicate to communication threads to stop
        self._com_thread_evt: threading.Event = None

    def _add_com_thread(self):
        """Method to add a background communication thread to the pool.

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

    def _stop_com_threads(self, timeout: int = 1):
        """Stop all background communication threads within timeout [s]."""
        if self._com_thread_evt:
            self._com_thread_evt.set()
            for component, thread in self._com_thread_pool.items():
                if thread.is_alive():
                    thread.join(timeout)
                    # check if thread is still alive
                    if thread.is_alive():
                        self.log.error(
                            "Could not join background communication thread for {} within timeout",
                            component,
                        )
                        raise RuntimeError(
                            f"Could not join running thread for {component} within timeout of {timeout}s!"
                        )
        self._com_thread_evt = None
        self._com_thread_pool = {}
