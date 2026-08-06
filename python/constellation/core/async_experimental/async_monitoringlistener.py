"""
SPDX-FileCopyrightText: 2026 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2

Async monitoring listener framework class.

Sits on top of AsyncCMDPListener and provides a high-level API for log
and metric subscriptions (both global and per-satellite), following the
C++ LogListener/StatListener pattern but merged into a single class to
match the sync Python MonitoringListener convention.
"""

import logging
from typing import Any
from uuid import UUID

from constellation.core.async_experimental.async_chirp import (
    AsyncCHIRPManager,
    CHIRPEvent,
    DiscoveredService,
)
from constellation.core.async_experimental.async_cmdplistener import AsyncCMDPListener
from constellation.core.async_experimental.async_pools import AsyncSubscriberPool
from constellation.core.chirp import CHIRPServiceIdentifier
from constellation.core.message.cmdp1 import (
    CMDP1LogMessage,
    CMDP1Message,
    CMDP1StatMessage,
)
from constellation.core.message.exceptions import MessageDecodingError

# Ascending severity order for log level topic generation
_LOG_LEVELS = ["TRACE", "DEBUG", "INFO", "WARNING", "STATUS", "CRITICAL"]


def _generate_log_topics(log_topic: str, level: str, subscribe: bool = True) -> list[str]:
    """Generate the CMDP topic strings for a log subscription.

    When subscribing, generates topics from the given level up to CRITICAL.
    When unsubscribing, generates topics from TRACE up to (but not including)
    the given level, matching the C++ LogListener::generate_topics pattern.
    """
    try:
        level_idx = _LOG_LEVELS.index(level.upper())
    except ValueError:
        return []

    if subscribe:
        levels = _LOG_LEVELS[level_idx:]
    else:
        levels = _LOG_LEVELS[:level_idx]

    topics = []
    for lvl in levels:
        if log_topic:
            topics.append(f"LOG/{lvl}/{log_topic.upper()}")
        else:
            topics.append(f"LOG/{lvl}")
    return topics


class AsyncMonitoringListener(AsyncCHIRPManager):
    """Async monitoring listener with log and metric subscription management.

    Inherits AsyncCHIRPManager for CHIRP discovery and adds CMDP monitoring
    via an AsyncCMDPListener backed by an AsyncSubscriberPool. Provides
    both global and per-satellite subscription APIs for logs and metrics.
    """

    def __init__(self, **kwds: Any) -> None:
        super().__init__(**kwds)
        self._cmdp_pool = AsyncSubscriberPool(
            self._async_ctx,
            callback=self._on_raw_cmdp_message,
        )
        self._cmdp_listener = AsyncCMDPListener(
            self._cmdp_pool,
            callback=self._on_cmdp_message,
        )
        self._global_log_level: str | None = None
        self.register_chirp_callback("monitoring_listener", self._on_monitoring_service)

    def _add_com_task(self) -> None:
        """Register the async CMDP pool coroutine."""
        super()._add_com_task()
        self._com_task_factories.append(self._cmdp_pool.run)

    # -- Raw message dispatch (pool callback -> CMDP listener) --

    def _on_raw_cmdp_message(self, uuid: UUID, frames: list[bytes]) -> None:
        """Route raw multipart frames through the CMDP listener for
        subscription tracking and notification interception."""
        self._cmdp_listener.handle_message(uuid, frames)

    # -- Decoded message dispatch (CMDP listener callback) --

    def _on_cmdp_message(self, msg: CMDP1Message) -> None:
        """Decode and dispatch a regular (non-notification) CMDP message."""
        try:
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
        except MessageDecodingError as e:
            self.log.debug("Failed to decode CMDP message: %s", e)

    # -- CHIRP integration --

    def _on_monitoring_service(self, event: CHIRPEvent, service: DiscoveredService) -> None:
        """Handle MONITORING service connect/disconnect."""
        if service.service_id != CHIRPServiceIdentifier.MONITORING:
            return
        if event == CHIRPEvent.SERVICE_CONNECTED:
            self._cmdp_pool.add_socket(service.host_id, service.addresses[0], service.port)
            self._cmdp_listener.on_host_connected(service.host_id)
        elif event == CHIRPEvent.SERVICE_DISCONNECTED:
            self._cmdp_listener.on_host_disconnected(service.host_id)
            self._cmdp_pool.remove_socket(service.host_id)

    # -- Global log subscription API --

    def set_global_log_level(self, level: str) -> None:
        """Subscribe to all log messages at the given level and above.

        Replaces any previous global log level subscription.
        """
        unsub = _generate_log_topics("", level, subscribe=False)
        sub = _generate_log_topics("", level, subscribe=True)
        self._cmdp_listener.multiscribe_topics(unsub, sub)
        self._global_log_level = level.upper()

    def get_global_log_level(self) -> str | None:
        """Return the current global log level, or None if not set."""
        return self._global_log_level

    def subscribe_log_topic(self, log_topic: str, level: str) -> None:
        """Subscribe to a specific log topic at the given level and above."""
        if not log_topic:
            return
        unsub = _generate_log_topics(log_topic, level, subscribe=False)
        sub = _generate_log_topics(log_topic, level, subscribe=True)
        self._cmdp_listener.multiscribe_topics(unsub, sub)

    def unsubscribe_log_topic(self, log_topic: str) -> None:
        """Unsubscribe from a specific log topic at all levels."""
        if not log_topic:
            return
        all_levels = _generate_log_topics(log_topic, "TRACE", subscribe=True)
        self._cmdp_listener.multiscribe_topics(all_levels, [])

    # -- Per-satellite log subscription API --

    def subscribe_extra_log_topic(self, host: str, log_topic: str, level: str) -> None:
        """Subscribe to a log topic at the given level for a specific host."""
        unsub = _generate_log_topics(log_topic, level, subscribe=False)
        sub = _generate_log_topics(log_topic, level, subscribe=True)
        self._cmdp_listener.multiscribe_extra_topics(host, unsub, sub)

    def unsubscribe_extra_log_topic(self, host: str, log_topic: str) -> None:
        """Unsubscribe from a log topic for a specific host."""
        all_levels = _generate_log_topics(log_topic, "TRACE", subscribe=True)
        self._cmdp_listener.multiscribe_extra_topics(host, all_levels, [])

    # -- Global metric subscription API --

    def subscribe_metric(self, metric: str, host: str | None = None) -> None:
        """Subscribe to a metric topic, globally or for a specific host."""
        topic = f"STAT/{metric}"
        if host is None:
            self._cmdp_listener.subscribe_topic(topic)
        else:
            self._cmdp_listener.subscribe_extra_topic(host, topic)

    def unsubscribe_metric(self, metric: str, host: str | None = None) -> None:
        """Unsubscribe from a metric topic, globally or for a specific host."""
        topic = f"STAT/{metric}"
        if host is None:
            self._cmdp_listener.unsubscribe_topic(topic)
        else:
            self._cmdp_listener.unsubscribe_extra_topic(host, topic)

    def get_metric_subscriptions(self, host: str | None = None) -> set[str]:
        """Return subscribed metric names (without STAT/ prefix)."""
        if host is None:
            raw = self._cmdp_listener.get_topic_subscriptions()
        else:
            raw = self._cmdp_listener.get_extra_topic_subscriptions(host)
        return {t[5:] for t in raw if t.startswith("STAT/") and t != "STAT?"}

    # -- Available topics and senders (delegated to CMDP listener) --

    def get_available_topics(self, sender: str | None = None) -> dict[str, str]:
        """Return available CMDP topics, optionally filtered by sender."""
        return self._cmdp_listener.get_available_topics(sender)

    def get_available_senders(self) -> set[str]:
        """Return the set of known CMDP senders."""
        return self._cmdp_listener.get_available_senders()

    # -- Backward-compatible set_topics passthrough --

    def set_topics(self, topics: list[str]) -> None:
        """Replace all global subscriptions.

        Provided for backward compatibility with TopicManager which
        computes the full topic set and pushes it in one call.
        """
        current = self._cmdp_listener.get_topic_subscriptions()
        new_set = set(topics)
        to_unsub = list(current - new_set)
        to_sub = list(new_set - current)
        self._cmdp_listener.multiscribe_topics(to_unsub, to_sub)

    # -- Callbacks for subclasses --

    def receive_log(self, record: logging.LogRecord) -> None:
        """Called when a log message arrives. Override in subclass."""

    def receive_metric(self, sender: str, metric, time, value) -> None:
        """Called when a metric arrives. Override in subclass."""

    def receive_notification(self, sender: str, prefix: str, topics: dict[str, str]) -> None:
        """Called when a notification arrives. Override in subclass."""
