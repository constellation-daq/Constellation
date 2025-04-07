"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2

This module provides a base class for Constellation Satellite modules.
"""

import atexit
import logging
import re
import socket
import threading
from argparse import ArgumentParser
from queue import Queue
from typing import Any, cast

import coloredlogs  # type: ignore[import-untyped]
import zmq

from . import __version__, __version_code_name__
from .cmdp import CMDPTransmitter
from .logging import ConstellationLogger, ZeroMQSocketLogHandler
from .network import get_interfaces, validate_interface


@atexit.register
def destroy_satellites() -> None:
    """Close down connections and perform orderly re-entry."""
    for sat in SATELLITE_LIST:
        sat.reentry()


class ConstellationArgumentParser(ArgumentParser):
    """Customized Argument parser providing basic Satellite options."""

    def __init__(self, *args: Any, **kwargs: Any):
        super().__init__(*args, **kwargs)
        # generic arguments
        self.add_argument(
            "-l",
            "--log-level",
            default="info",
            help="The maximum level of log messages to print to the console.",
        )
        self.add_argument("--version", action="version", version=f"Constellation v{__version__} ({__version_code_name__})")
        # add a constellation argument group
        self.constellation = self.add_argument_group("Constellation")
        self.constellation.add_argument(
            "--name",
            "-n",
            default=socket.gethostname().replace("-", "_").replace(".", "_"),
            type=str,
            help="The name of the Satellite. This has to be unique within "
            "the Constellation group. Together with the Satellite class, "
            "this forms the Canonical Name.",
        )
        self.constellation.add_argument(
            "--group",
            "-g",
            required=True,
            type=str,
            help="The Constellation group to connect to. This separates "
            "different Constellations running on the same network",
        )
        # add a networking argument group
        self.network = self.add_argument_group("Network configuration")
        self.network.add_argument(
            "--interface",
            type=validate_interface,
            choices=get_interfaces(),
            default="*",
            help="The network interface (i.e. IP address) to bind to. "
            "Use '*' to bind to alla available interfaces "
            "(default: %(default)s).",
        )


EPILOG = "This command is part of the Constellation Python core package."


class BaseSatelliteFrame:
    """Base class for all Satellite components to inherit from.

    Provides the basic internal Satellite infrastructure related to logging, ZMQ
    and threading that all Satellite communication service components (i.e.
    mixin classes) share.

    """

    def __init__(self, name: str, interface: str, mon_port: int | None = None, **_kwds: Any):
        # type name == python class name
        self.type = type(self).__name__
        # Check if provided name is valid:
        if not re.match(r"^\w+$", name):
            raise ValueError("Satellite name contains invalid characters")
        # add type name to create the canonical name
        self.name = f"{self.type}.{name}"
        self.context = zmq.Context()
        self.interface = interface

        cmdp_socket = self.context.socket(zmq.PUB)
        if not mon_port:
            self.mon_port = cmdp_socket.bind_to_random_port(f"tcp://{interface}")
        else:
            cmdp_socket.bind(f"tcp://{interface}:{mon_port}")
            self.mon_port = mon_port
        self._cmdp_transmitter = CMDPTransmitter(self.name, cmdp_socket)
        self._zmq_log_handler = ZeroMQSocketLogHandler(self._cmdp_transmitter)

        self.log = self.get_logger(self.type.upper())

        # Set up a queue for handling tasks related to incoming requests via
        # CSCP or offers via CHIRP. This makes sure that these can be performed
        # thread-safe from the main thread of the Satellite. A 'task' consists
        # of a tuple consisting of a callback method and an object passed as
        # argument (DiscoveredService and CSCPMessage for CHIRP and CSCP,
        # respectively).
        self.task_queue: Queue = Queue()  # type: ignore[type-arg]

        # dict to keep references to all communication service threads usually
        # running in the background
        self._com_thread_pool: dict[str, threading.Thread] = {}
        # Event to indicate to communication service threads to stop
        self._com_thread_evt: threading.Event | None = None

        # add self to list of satellites to destroy on shutdown
        global SATELLITE_LIST  # noqa
        SATELLITE_LIST.append(self)

    def get_logger(self, name: str) -> ConstellationLogger:
        logging.setLoggerClass(ConstellationLogger)
        logger = cast(ConstellationLogger, logging.getLogger(name))
        logger.addHandler(self._zmq_log_handler)
        logger.setLevel(logging.DEBUG)
        coloredlogs.install(logger=logger, level=coloredlogs.DEFAULT_LOG_LEVEL)
        return logger

    def _add_com_thread(self) -> None:
        """Method to add a background communication service thread to the pool.

        Does nothing in the base class.

        """
        self.log.debug("Satellite Base class _add_thread called")
        pass

    def _start_com_threads(self) -> None:
        """Start all background communication threads."""
        self._com_thread_evt = threading.Event()
        for component, thread in self._com_thread_pool.items():
            self.log.debug("Starting thread for %s communication", component)
            thread.start()

    def _stop_com_threads(self, timeout: float = 1.5) -> None:
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
                        raise RuntimeError(f"Could not join running thread for {component} within timeout of {timeout}s!")
        self._com_thread_evt = None
        self._com_thread_pool = {}

    def reentry(self) -> None:
        """Orderly destroy the satellite."""
        self.log.debug("Stopping all communication threads.")
        self._stop_com_threads()
        self.log.debug("Terminating ZMQ context.")
        self._zmq_log_handler.close()
        self.context.term()


SATELLITE_LIST: list[BaseSatelliteFrame] = []
