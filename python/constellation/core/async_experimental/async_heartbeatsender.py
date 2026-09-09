"""
SPDX-FileCopyrightText: 2026 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2

Async heartbeat sender using Constellation Heartbeat Protocol (CHP).
"""

import asyncio
import io
import math
import time
from typing import Any

import msgpack  # type: ignore[import-untyped]
import zmq
import zmq.asyncio

from constellation.core.base import BaseSatelliteFrame
from constellation.core.chp import CHPMessageFlags, CHPRole
from constellation.core.commandmanager import cscp_requestable
from constellation.core.fsm import SatelliteFSM
from constellation.core.message.cscp1 import CSCP1Message
from constellation.core.protocol import Protocol
from constellation.core.protocol.cscp1 import SatelliteState


class AsyncCHPTransmitter:
    """Async CHP transmitter using zmq.asyncio XPUB socket."""

    def __init__(self, name: str, socket: zmq.asyncio.Socket) -> None:
        self.name = name
        self._socket = socket

    async def send(
        self,
        state: int,
        interval: int,
        msgflags: CHPMessageFlags,
        status: str | None = None,
    ) -> None:
        """Send state and interval via CHP."""
        stream = io.BytesIO()
        packer = msgpack.Packer()
        stream.write(packer.pack(Protocol.CHP1))
        stream.write(packer.pack(self.name))
        stream.write(packer.pack(msgpack.Timestamp.from_unix_nano(time.time_ns())))
        stream.write(packer.pack(state))
        stream.write(packer.pack(msgflags))
        stream.write(packer.pack(interval))

        if status:
            await self._socket.send(stream.getbuffer(), flags=zmq.SNDMORE)
            await self._socket.send_string(status)
        else:
            await self._socket.send(stream.getbuffer())

    async def parse_subscriptions(self) -> int:
        """Parse pending subscription and unsubscription messages."""
        subscriptions = 0
        while True:
            try:
                msg = await self._socket.recv(zmq.NOBLOCK)
                subscriptions += 1 if msg == b"\x01" else -1
            except zmq.ZMQError:
                break
        return subscriptions

    def close(self) -> None:
        """Close the XPUB socket."""
        self._socket.close()


class AsyncHeartbeatSender:
    """Send regular state updates via CHP using asyncio."""

    DEFAULT_PERIOD_MS = 30000
    MINIMUM_PERIOD_MS = 500

    def __init__(
        self,
        name: str,
        fsm: SatelliteFSM,
        ctx: zmq.asyncio.Context,
        hb_port: int = 0,
        logger: Any = None,
    ) -> None:
        self._fsm = fsm
        self._logger = logger
        self._default_period = self.DEFAULT_PERIOD_MS
        self._period = self.MINIMUM_PERIOD_MS
        self._subscribers = 0
        self._role = CHPRole.DYNAMIC

        socket = ctx.socket(zmq.XPUB)
        socket.setsockopt(zmq.XPUB_VERBOSER, True)
        socket.setsockopt(zmq.LINGER, 2000)
        socket.setsockopt(zmq.RCVTIMEO, 5000)

        if not hb_port:
            self.hb_port = socket.bind_to_random_port("tcp://*")
        else:
            socket.bind(f"tcp://*:{hb_port}")
            self.hb_port = hb_port

        if self._logger:
            self._logger.info(f"Async heartbeat sender on port {self.hb_port}")

        self._transmitter = AsyncCHPTransmitter(name, socket)

    @property
    def role(self) -> CHPRole:
        return self._role

    @role.setter
    def role(self, new_role: CHPRole) -> None:
        self._role = new_role

    @property
    def max_heartbeat_interval(self) -> int:
        return int(self._default_period / 1000)

    @max_heartbeat_interval.setter
    def max_heartbeat_interval(self, new_period: int) -> None:
        if self._logger:
            self._logger.debug(f"Adjusting maximum heartbeat interval to {new_period} seconds.")
        self._default_period = new_period * 1000

    async def send_extrasystole(self, state: SatelliteState) -> None:
        """Send an immediate heartbeat on state change."""
        if self._logger:
            self._logger.trace("Sending extrasystole")
        await self._transmitter.send(
            state.value,
            self._period,
            self._role.flags() | CHPMessageFlags.IS_EXTRASYSTOLE,
            self._fsm.status,
        )

    async def run(self, stop: asyncio.Event) -> None:
        """Run the periodic heartbeat loop until stop is set."""
        if self._logger:
            self._logger.info("Starting async heartbeat sender")
        last = time.monotonic()
        prev_status = self._fsm.status

        while not stop.is_set():
            elapsed = time.monotonic() - last
            wait_seconds = (self._period * 0.8 / 1000) - elapsed
            if wait_seconds > 0:
                try:
                    await asyncio.wait_for(stop.wait(), timeout=min(wait_seconds, 0.1))
                except TimeoutError:
                    pass
                continue

            # Update subscriber count and adjust heartbeat period
            self._subscribers += await self._transmitter.parse_subscriptions()
            self._period = min(
                self._default_period,
                max(
                    self.MINIMUM_PERIOD_MS,
                    int(self.MINIMUM_PERIOD_MS * math.sqrt(max(self._subscribers, 1) - 1) * 3),
                ),
            )

            if self._logger:
                self._logger.trace(
                    f"Sending heartbeat, current period {self._period}ms with {self._subscribers} subscribers"
                )

            last = time.monotonic()
            state = self._fsm.state
            current_status = self._fsm.status if self._fsm.status != prev_status else None
            await self._transmitter.send(
                state.value,
                self._period,
                self._role.flags(),
                current_status,
            )
            prev_status = self._fsm.status

        if self._logger:
            self._logger.info("Heartbeat sender shutting down")
        self._transmitter.close()

    def close(self) -> None:
        """Close the transmitter socket."""
        self._transmitter.close()


class AsyncHeartbeatSenderMixin(BaseSatelliteFrame):
    """Mixin integrating AsyncHeartbeatSender with BaseSatelliteFrame."""

    def __init__(self, hb_port: int = 0, **kwds: Any) -> None:
        super().__init__(**kwds)
        self._hb_sender = AsyncHeartbeatSender(
            name=self.name,
            fsm=self.fsm,
            ctx=self._async_ctx,
            hb_port=hb_port,
            logger=self.get_logger("LINK"),
        )
        self.hb_port = self._hb_sender.hb_port
        self.register_state_callback("heartbeater", self._async_extrasystole)

    def _async_extrasystole(self, state: SatelliteState) -> None:
        """Schedule an extrasystole coroutine on the running event loop."""
        try:
            loop = asyncio.get_running_loop()
            loop.create_task(self._hb_sender.send_extrasystole(state))
        except RuntimeError:
            pass

    def _add_com_task(self) -> None:
        """Register the heartbeat sender coroutine."""
        super()._add_com_task()
        self._com_task_factories.append(self._hb_sender.run)

    @property
    def heartbeat_role(self) -> CHPRole:
        return self._hb_sender.role

    @heartbeat_role.setter
    def heartbeat_role(self, new_role: CHPRole) -> None:
        self._hb_sender.role = new_role

    @cscp_requestable()
    def get_role(self, _request: CSCP1Message | None = None) -> tuple[str, Any, dict[str, Any]]:
        """Return the current role of the Satellite.

        No payload argument.
        """
        return self._hb_sender.role.name, self._hb_sender.role.flags().value, {}
