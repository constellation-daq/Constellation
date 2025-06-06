"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2
"""

import math
import threading
import time
from datetime import datetime
from typing import Any

import zmq

from .chp import CHPTransmitter
from .fsm import SatelliteStateHandler


class HeartbeatSender(SatelliteStateHandler):
    """Send regular state updates via Constellation Heartbeat Protocol."""

    def __init__(
        self,
        name: str,
        hb_port: int,
        interface: str,
        **kwargs: Any,
    ) -> None:

        super().__init__(name=name, interface=interface, **kwargs)
        self.default_period = 30000
        self.minimum_period = 500
        self._heartbeat_period = 500
        self._subscribers = 0

        self.log_chp_s = self.get_logger("CHP")

        # register and start heartbeater
        socket = self.context.socket(zmq.XPUB)

        # switch to verbose xpub mode to receive all subscription & unsubscription messages:
        socket.setsockopt(zmq.XPUB_VERBOSER, True)

        if not hb_port:
            self.hb_port = socket.bind_to_random_port(f"tcp://{interface}")
        else:
            socket.bind(f"tcp://{interface}:{hb_port}")
            self.hb_port = hb_port

        self.log_chp_s.info(f"Setting up heartbeater on port {self.hb_port}")
        self._hb_tm = CHPTransmitter(self.name, socket)

    def _add_com_thread(self) -> None:
        """Add the CHIRP broadcaster thread to the communication thread pool."""
        super()._add_com_thread()
        self._com_thread_pool["heartbeat"] = threading.Thread(target=self._run_heartbeat, daemon=True)
        self.log_chp_s.debug("Heartbeat sender thread prepared and added to the pool.")

    def _run_heartbeat(self) -> None:
        self.log_chp_s.info("Starting heartbeat sender thread")
        last = datetime.now()
        # assert for mypy static type analysis
        assert isinstance(self._com_thread_evt, threading.Event), "Thread Event not set up correctly"
        while not self._com_thread_evt.is_set():
            # Wait until we need to send the next heartbeat, stay 20% below configured interval
            if ((datetime.now() - last).total_seconds() > self._heartbeat_period * 0.8 / 1000) or self.fsm.transitioned:
                # Update number of subscribers and the associated heartbeat period
                self._subscribers += self._hb_tm.parse_subscriptions()
                self._heartbeat_period = min(
                    self.default_period,
                    max(self.minimum_period, int(self.minimum_period * math.sqrt(max(self._subscribers, 1) - 1) * 3)),
                )
                self.log_chp_s.trace(
                    "Sending heartbeat, current period "
                    + str(self._heartbeat_period)
                    + "ms with "
                    + str(self._subscribers)
                    + " subscribers"
                )

                last = datetime.now()
                state = self.fsm.current_state_value
                self._hb_tm.send(state.value, self._heartbeat_period)
                self.fsm.transitioned = False
            else:
                time.sleep(0.1)
        self.log_chp_s.info("HeartbeatSender thread shutting down.")
        # clean up
        self._hb_tm.close()
