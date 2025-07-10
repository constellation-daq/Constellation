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

from .chp import CHPRole, CHPTransmitter
from .commandmanager import cscp_requestable
from .fsm import SatelliteStateHandler
from .message.cscp1 import CSCP1Message


class HeartbeatSender(SatelliteStateHandler):
    """Send regular state updates via Constellation Heartbeat Protocol."""

    def __init__(
        self,
        name: str,
        hb_port: int,
        **kwargs: Any,
    ) -> None:

        super().__init__(name=name, **kwargs)
        self.default_heartbeat_period = 30000
        self.minimum_heartbeat_period = 500
        self._heartbeat_period = 500
        self._subscribers = 0
        self._role = CHPRole.DYNAMIC

        self.log_chp_s = self.get_logger("LINK")

        # register and start heartbeater
        socket = self.context.socket(zmq.XPUB)

        # switch to verbose xpub mode to receive all subscription & unsubscription messages:
        socket.setsockopt(zmq.XPUB_VERBOSER, True)

        if not hb_port:
            self.hb_port = socket.bind_to_random_port("tcp://*")
        else:
            socket.bind(f"tcp://*:{hb_port}")
            self.hb_port = hb_port

        self.log_chp_s.info(f"Setting up heartbeater on port {self.hb_port}")
        self._hb_tm = CHPTransmitter(self.name, socket)

    @property
    def role(self) -> CHPRole:
        return self._role

    @role.setter
    def role(self, new_role: CHPRole) -> None:
        self._role = new_role

    def _add_com_thread(self) -> None:
        """Add the heartbeat thread to the communication thread pool."""
        super()._add_com_thread()
        self._com_thread_pool["heartbeat"] = threading.Thread(target=self._run_heartbeat, daemon=True)
        self.log_chp_s.debug("Heartbeat sender thread prepared and added to the pool.")

    def _run_heartbeat(self) -> None:
        self.log_chp_s.info("Starting heartbeat sender thread")
        last = datetime.now()
        # assert for mypy static type analysis
        assert isinstance(self._com_thread_evt, threading.Event), "Thread Event not set up correctly"

        prev_status = self.fsm.status
        while not self._com_thread_evt.is_set():
            # Wait until we need to send the next heartbeat, stay 20% below configured interval
            if ((datetime.now() - last).total_seconds() > self._heartbeat_period * 0.8 / 1000) or self.fsm.transitioned:
                # Update number of subscribers and the associated heartbeat period
                self._subscribers += self._hb_tm.parse_subscriptions()
                self._heartbeat_period = min(
                    self.default_heartbeat_period,
                    max(
                        self.minimum_heartbeat_period,
                        int(self.minimum_heartbeat_period * math.sqrt(max(self._subscribers, 1) - 1) * 3),
                    ),
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
                self._hb_tm.send(
                    state.value,
                    self._heartbeat_period,
                    self._role.flags(),
                    self.fsm.status if self.fsm.status != prev_status else None,
                )
                self.fsm.transitioned = False
                prev_status = self.fsm.status
            else:
                time.sleep(0.1)
        self.log_chp_s.info("HeartbeatSender thread shutting down.")
        # clean up
        self._hb_tm.close()

    @cscp_requestable
    def get_role(self, _request: CSCP1Message | None = None) -> tuple[str, Any, dict[str, Any]]:
        """Return the current role of the Satellite.

        No payload argument.
        """
        return self._role.name, self._role.flags().value, {}
