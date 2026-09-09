"""
SPDX-FileCopyrightText: 2026 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2

Async CMDP listener following the C++ CMDPListener API.

Manages an AsyncSubscriberPool for MONITORING sockets and adds reference
counted topic subscriptions (global and per-satellite "extra" topics),
available topic tracking from CMDP notification messages, and sender
lifecycle hooks.

Global topics apply to every connected socket. Extra topics are additional
subscriptions for a specific host. The interaction between the two layers
follows the same rules as the C++ implementation:
  - subscribeExtraTopic only issues a ZMQ subscribe if the topic is not
    already covered by a global subscription
  - unsubscribeTopic re-subscribes any extra topics that overlap with
    the removed global topic
  - host_connected subscribes the new socket to all current global and
    applicable extra topics
"""

import logging
from collections.abc import Callable
from uuid import UUID

from constellation.core.async_experimental.async_pools import AsyncSubscriberPool
from constellation.core.message.cmdp1 import CMDP1Message, CMDP1Notification
from constellation.core.message.exceptions import MessageDecodingError

_log = logging.getLogger(__name__)


class AsyncCMDPListener:
    """Subscription-tracked CMDP listener with per-satellite extras.

    Wraps an AsyncSubscriberPool and manages two layers of subscriptions:
    global topics (applied to all sockets) and per-host extra topics.
    Intercepts CMDP notification messages for available topic tracking
    and forwards regular messages to the user callback.
    """

    def __init__(
        self,
        pool: AsyncSubscriberPool,
        callback: Callable[[CMDP1Message], None],
    ) -> None:
        self._pool = pool
        self._user_callback = callback

        # Global subscriptions tracked as a set of topic strings
        self._subscribed_topics: set[str] = set()

        # Per-host extra subscriptions, keyed by canonical name
        self._extra_subscribed_topics: dict[str, set[str]] = {}

        # Available topics from notifications, keyed by canonical name
        self._available_topics: dict[str, dict[str, str]] = {}

        # Map host_id (UUID) to canonical name for host_connected/disconnected
        self._host_names: dict[UUID, str] = {}

    # Global topic management

    def subscribe_topic(self, topic: str) -> None:
        """Subscribe to a topic on all sockets."""
        self.multiscribe_topics([], [topic])

    def unsubscribe_topic(self, topic: str) -> None:
        """Unsubscribe from a topic on all sockets."""
        self.multiscribe_topics([topic], [])

    def multiscribe_topics(
        self,
        unsubscribe_topics: list[str],
        subscribe_topics: list[str],
    ) -> None:
        """Atomically unsubscribe and subscribe multiple global topics.

        After removing global topics, any extra topics that overlap with
        the removed globals are re-subscribed on their specific hosts to
        maintain correct per-socket subscription state.
        """
        actually_unsubscribed: set[str] = set()

        for topic in unsubscribe_topics:
            if topic in self._subscribed_topics:
                self._subscribed_topics.discard(topic)
                self._pool.unsubscribe(topic)
                actually_unsubscribed.add(topic)

        for topic in subscribe_topics:
            if topic not in self._subscribed_topics:
                self._subscribed_topics.add(topic)
                self._pool.subscribe(topic)

        # Re-subscribe extra topics that were covered by removed globals
        if actually_unsubscribed:
            for host_name, extra_topics in self._extra_subscribed_topics.items():
                for topic in extra_topics:
                    if topic in actually_unsubscribed:
                        self._pool.subscribe(topic, host_name)

    def get_topic_subscriptions(self) -> set[str]:
        """Return the set of currently subscribed global topics."""
        return set(self._subscribed_topics)

    # Per-satellite extra topic management

    def subscribe_extra_topic(self, host: str, topic: str) -> None:
        """Subscribe to an extra topic for a specific host."""
        self.multiscribe_extra_topics(host, [], [topic])

    def unsubscribe_extra_topic(self, host: str, topic: str) -> None:
        """Unsubscribe from an extra topic for a specific host."""
        self.multiscribe_extra_topics(host, [topic], [])

    def multiscribe_extra_topics(
        self,
        host: str,
        unsubscribe_topics: list[str],
        subscribe_topics: list[str],
    ) -> None:
        """Atomically manage extra topics for a specific host.

        Subscribes are skipped if the topic is already in the global set.
        Unsubscribes are skipped if the topic is still in the global set.
        """
        if host not in self._extra_subscribed_topics:
            # First time for this host, subscribe to each new topic
            topics: set[str] = set()
            for topic in subscribe_topics:
                topics.add(topic)
                if topic not in self._subscribed_topics:
                    self._pool.subscribe(topic, host)
            self._extra_subscribed_topics[host] = topics
        else:
            host_topics = self._extra_subscribed_topics[host]
            for topic in unsubscribe_topics:
                if topic in host_topics:
                    host_topics.discard(topic)
                    if topic not in self._subscribed_topics:
                        self._pool.unsubscribe(topic, host)
            for topic in subscribe_topics:
                if topic not in host_topics:
                    host_topics.add(topic)
                    if topic not in self._subscribed_topics:
                        self._pool.subscribe(topic, host)

    def get_extra_topic_subscriptions(self, host: str) -> set[str]:
        """Return the set of extra topics for a specific host."""
        return set(self._extra_subscribed_topics.get(host, set()))

    def remove_extra_topic_subscriptions(self, host: str | None = None) -> None:
        """Remove extra topics for one host or all hosts.

        Unsubscribes only those topics not covered by global subscriptions.
        """
        if host is not None:
            extras = self._extra_subscribed_topics.pop(host, None)
            if extras:
                for topic in extras:
                    if topic not in self._subscribed_topics:
                        self._pool.unsubscribe(topic, host)
        else:
            for host_name, extras in self._extra_subscribed_topics.items():
                for topic in extras:
                    if topic not in self._subscribed_topics:
                        self._pool.unsubscribe(topic, host_name)
            self._extra_subscribed_topics.clear()

    # Available topic tracking

    def get_available_topics(self, sender: str | None = None) -> dict[str, str]:
        """Return available topics, optionally filtered by sender."""
        if sender is not None:
            return dict(self._available_topics.get(sender, {}))
        merged: dict[str, str] = {}
        for sender_topics in self._available_topics.values():
            merged.update(sender_topics)
        return merged

    def get_available_senders(self) -> set[str]:
        """Return the set of known CMDP senders."""
        return set(self._available_topics.keys())

    def is_topic_available(self, topic: str) -> bool:
        """Check if a topic has been seen from any sender."""
        return any(topic in t for t in self._available_topics.values())

    def is_sender_available(self, sender: str) -> bool:
        """Check if a sender is known."""
        return sender in self._available_topics

    # CHIRP integration (called by AsyncMonitoringListener)

    def on_host_connected(self, uuid: UUID, canonical_name: str | None = None) -> None:
        """Subscribe a newly connected socket to all applicable topics.

        Called by the CHIRP callback when a MONITORING service is discovered.
        The canonical_name is stored for sender lifecycle tracking.
        """
        if canonical_name:
            self._host_names[uuid] = canonical_name

        # Subscribe to all global topics
        for topic in self._subscribed_topics:
            self._pool.subscribe(topic, uuid)

        # Subscribe to applicable extra topics
        name = self._host_names.get(uuid)
        if name and name in self._extra_subscribed_topics:
            for topic in self._extra_subscribed_topics[name]:
                if topic not in self._subscribed_topics:
                    self._pool.subscribe(topic, uuid)

    def on_host_disconnected(self, uuid: UUID) -> None:
        """Clean up available topics for a disconnecting host.

        Called by the CHIRP callback when a MONITORING service departs.
        """
        name = self._host_names.pop(uuid, None)
        if name is None:
            return

        if name in self._available_topics:
            del self._available_topics[name]
            self.sender_disconnected(name)

    # Message handling (called from pool callback)

    def handle_message(self, uuid: UUID, frames: list[bytes]) -> None:
        """Decode a raw CMDP multipart message and route it.

        Notifications are intercepted for available topic tracking.
        Regular messages are forwarded to the user callback.
        """
        try:
            msg = CMDP1Message.disassemble(frames)
        except MessageDecodingError as e:
            _log.debug("Failed to decode CMDP message from %s: %s", uuid, e)
            return

        if msg.is_notification():
            self._handle_notification(msg)
        else:
            self._track_topic_from_message(msg)
            self._user_callback(msg)

    def _handle_notification(self, msg: CMDP1Message) -> None:
        """Process a CMDP notification message updating available topics."""
        try:
            notification = CMDP1Notification.from_cmdp_message(msg)
        except MessageDecodingError:
            return

        sender = notification.sender
        topics = notification.topics
        new_sender = sender not in self._available_topics

        if new_sender:
            self._available_topics[sender] = {}

        new_topics = False
        for topic_name, description in topics.items():
            if topic_name not in self._available_topics[sender]:
                new_topics = True
            self._available_topics[sender][topic_name] = description

        if new_sender:
            self.sender_connected(sender)
        if new_topics:
            self.topics_changed(sender)

    def _track_topic_from_message(self, msg: CMDP1Message) -> None:
        """Track topics discovered from regular (non-notification) messages."""
        sender = msg.sender
        topic = msg.cmdp_topic
        new_sender = sender not in self._available_topics

        if new_sender:
            self._available_topics[sender] = {}

        new_topic = topic not in self._available_topics[sender]
        if new_topic:
            self._available_topics[sender][topic] = ""

        if new_sender:
            self.sender_connected(sender)
        if new_topic:
            self.topics_changed(sender)

    # Hooks for subclasses

    def topics_changed(self, sender: str) -> None:
        """Called when a sender's available topic list changes. Override in subclass."""

    def sender_connected(self, sender: str) -> None:
        """Called when a new CMDP sender is first seen. Override in subclass."""

    def sender_disconnected(self, sender: str) -> None:
        """Called when a CMDP sender disconnects. Override in subclass."""
