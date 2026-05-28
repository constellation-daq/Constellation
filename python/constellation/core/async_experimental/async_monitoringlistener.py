"""
SPDX-FileCopyrightText: 2026 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2

Async monitoring listener framework class.
"""

import logging
from typing import Any
from uuid import UUID

from ..message.cmdp1 import CMDP1LogMessage, CMDP1Message, CMDP1StatMessage
from .async_chirp import CHIRPEvent, DiscoveredService
from .async_chirpmanager import AsyncCHIRPManager
from .async_pools import AsyncSubscriberPool


class AsyncMonitoringListener(AsyncCHIRPManager):
    """Async equivalent of MonitoringListener.

    Inherits AsyncCHIRPManager and adds CMDP log/metric receiving
    via AsyncSubscriberPool.
    """

    def __init__(self, **kwds: Any) -> None:
        super().__init__(**kwds)
        self._cmdp_pool = AsyncSubscriberPool(
            self._async_ctx,
            callback=self._on_cmdp_message,
        )
        self.register_chirp_callback("monitoring_listener", self._on_monitoring_service)

    def _add_com_task(self) -> None:
        """Register the async CMDP pool coroutine."""
        super()._add_com_task()
        self._com_task_factories.append(self._cmdp_pool.run)

    def set_topics(self, topics: list[str]) -> None:
        """Set CMDP topic subscriptions across all sockets."""
        self._cmdp_pool.set_topics(topics)

    def _on_monitoring_service(self, event: CHIRPEvent, service: DiscoveredService) -> None:
        """Handle MONITORING service connect/disconnect."""
        if event == CHIRPEvent.SERVICE_CONNECTED:
            self._cmdp_pool.add_socket(service.host_id, service.addresses[0], service.port)
        elif event == CHIRPEvent.SERVICE_DISCONNECTED:
            self._cmdp_pool.remove_socket(service.host_id)

    def _on_cmdp_message(self, uuid: UUID, frames: list[bytes]) -> None:
        """Decode and dispatch an incoming CMDP message."""
        try:
            msg = CMDP1Message.disassemble(frames)
            if msg.is_log_message():
                log_msg = CMDP1LogMessage.from_cmdp_message(msg)
                self.receive_log(log_msg.to_log_record())
            elif msg.is_stat_message():
                stat_msg = CMDP1StatMessage.from_cmdp_message(msg)
                self.receive_metric(
                    stat_msg.sender,
                    stat_msg.metric,
                    stat_msg.time,
                    stat_msg.value,
                )
        except Exception:
            pass

    def receive_log(self, record: logging.LogRecord) -> None:
        """Called when a log message arrives. Override in subclass."""

    def receive_metric(self, sender: str, metric, time, value) -> None:
        """Called when a metric arrives. Override in subclass."""
