"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2
"""

import logging
import threading
import time
from collections.abc import Callable
from dataclasses import dataclass
from datetime import datetime
from functools import partial, wraps
from typing import Any, ParamSpec, TypeVar, cast

import zmq

from .base import BaseSatelliteFrame
from .cmdp import CMDPPublisher
from .logging import ConstellationLogger, ZeroMQSocketLogHandler
from .protocol.cmdp1 import Metric
from .protocol.cscp1 import SatelliteState, states_except

T = TypeVar("T")
P = ParamSpec("P")

DEFAULT_ALLOWED_STATES = states_except(
    [SatelliteState.NEW, SatelliteState.initializing, SatelliteState.reconfiguring, SatelliteState.ERROR]
)


@dataclass
class ScheduledMetric(Metric):
    interval: float
    value_cb: Callable[[], Any]
    allowed_states: list[SatelliteState] | None


def schedule_metric(
    unit: str, interval: float, allowed_states: list[SatelliteState] | None = DEFAULT_ALLOWED_STATES
) -> Callable[[Callable[P, T]], Callable[P, T]]:
    """Schedule a metric for a function with given interval in seconds"""

    def decorator(func: Callable[P, T]) -> Callable[P, T]:
        @wraps(func)
        def wrapper(*args: P.args, **kwargs: P.kwargs) -> T:
            # Note: this method stll takes self as an argument -> added in _get_scheduled_metrics
            return func(*args, **kwargs)

        # Mark function as scheduled metric
        metric = ScheduledMetric(
            func.__name__, unit, func.__doc__ if func.__doc__ else "", interval, lambda: None, allowed_states
        )
        setattr(metric, "raw_value_cb", wrapper)  # noqa: B010
        setattr(wrapper, "metric", metric)  # noqa: B010

        return wrapper

    return decorator


class MonitoringSender(BaseSatelliteFrame):
    """Sender mixin class for Constellation Monitoring Distribution Protocol.

    Any method of inheriting classes that has the @schedule_metric decorator,
    will be regularly polled for new values and a corresponding Metric be sent
    on the monitoring port.

    """

    def __init__(self, mon_port: int | None = None, **kwds: Any):
        """Set up logging and metrics transmitters."""
        super().__init__(**kwds)

        self.log_cmdp_s = self.get_logger("MNTR")

        # Open ZMQ socket using (X)PUB/SUB pattern. XPUB allows us to handle
        # subscription messages directly.
        cmdp_socket = self.context.socket(zmq.XPUB)
        # Set linger period for socket shutdown to avoid long hangs shutting
        # down [ms]
        cmdp_socket.setsockopt(zmq.LINGER, 2000)
        # Set maximum time before a recv operation returns with EAGAIN [ms]
        cmdp_socket.setsockopt(zmq.RCVTIMEO, 5000)

        if not mon_port:
            self.mon_port = cmdp_socket.bind_to_random_port("tcp://*")
        else:
            cmdp_socket.bind(f"tcp://*:{mon_port}")
            self.mon_port = mon_port
        # Instantiate CMDP publish transmitter and CMDP log handler
        self._cmdp_publisher = CMDPPublisher(self.name, cmdp_socket)
        self._zmq_log_handler = ZeroMQSocketLogHandler()
        for handler in logging.root.handlers:
            if isinstance(handler, ZeroMQSocketLogHandler):
                self._zmq_log_handler = handler
                break
        self._zmq_log_handler.transmitter = self._cmdp_publisher

        # Store registered metrics
        self._metrics: dict[str, Metric | ScheduledMetric] = {}

        # Add and register metrics added via function decorator
        self._metrics.update(self._get_scheduled_metrics())
        for metric_name, metric in self._metrics.items():
            self._cmdp_publisher.register_stat(metric_name, metric.description)

        self._zmq_log_handler.setLevel("TRACE")
        # add zmq logging to existing Constellation loggers
        for logger in logging.root.manager.loggerDict.values():
            # only configure Constellation's own loggers
            if isinstance(logger, ConstellationLogger):
                # only configure logger if we keep a reference to it ourselves
                if logger in self.__dict__.values():
                    self._configure_cmdp_logger(logger)
        # update list of subscribers (both log and metric)
        self._cmdp_publisher.update_subscriptions()

    def _configure_cmdp_logger(self, logger: ConstellationLogger) -> None:
        """Configure log handler for CMDP messaging via ZMQ."""
        self._cmdp_publisher.register_log(logger.name)

    def register_metric(self, name: str, unit: str, description: str) -> None:
        """Register a manually triggered metric"""
        self._cmdp_publisher.register_stat(name, description)
        metric = Metric(name, unit, description)
        self._metrics[metric.name] = metric

    def register_scheduled_metric(
        self,
        name: str,
        unit: str,
        description: str,
        interval: float,
        value_cb: Callable[[], Any],
        allowed_states: list[SatelliteState] = DEFAULT_ALLOWED_STATES,
    ) -> None:
        """Register a scheduled metric"""
        self._cmdp_publisher.register_stat(name, description)
        metric = ScheduledMetric(name, unit, description, interval, value_cb, allowed_states)
        self._metrics[metric.name] = metric

    def unregister_metric(self, metric_name: str) -> None:
        """Removes a previously registered metric"""
        self._metrics.pop(metric_name, None)
        self._cmdp_publisher.unregister_stat(metric_name)

    def reset_metrics(self) -> None:
        """Reset all metric

        This removes all metrics and resets it to the ones added via the `@schedule_metric` decorator.
        """
        decorated_metrics = self._get_scheduled_metrics()
        for metric_name in self._metrics:
            if metric_name not in decorated_metrics:
                self._cmdp_publisher.unregister_stat(metric_name)
        self._metrics = {}
        self._metrics.update(decorated_metrics)

    def should_stat(self, metric_name) -> bool:
        """Checks if a metric has any subscribers"""
        return self._cmdp_publisher.has_metric_subscribers(metric_name)

    def should_log(self, levelname: str, topic: str | None = None) -> bool:
        """Checks if a log level or topic has any subscribers"""
        return self._cmdp_publisher.has_log_subscribers(levelname, topic)

    def stat(self, metric_name: str, value: Any) -> None:
        """Manually emit a registered metric"""
        try:
            metric = self._metrics[metric_name]
            self.log_cmdp_s.trace(f"Sending metric {metric_name} with value {value}{metric.unit}")
            self._cmdp_publisher.send_metric(self._metrics[metric_name], value)
        except KeyError:
            self.log_cmdp_s.warning(f"Cannot stat {metric_name}: metric not registered")

    def _get_scheduled_metrics(self) -> dict[str, ScheduledMetric]:
        """Loop over all class methods and return those marked as scheduled metric"""
        res = {}
        for attr in dir(self):
            obj = getattr(self, attr)
            if callable(obj):
                if hasattr(obj, "metric") and hasattr(obj, "__name__"):
                    metric = getattr(obj, "metric")  # noqa: B009
                    metric.value_cb = partial(getattr(metric, "raw_value_cb"), self)  # noqa: B009
                    res[obj.__name__] = metric
        return res

    def _add_com_thread(self) -> None:
        """Add the metric sender thread to the communication thread pool."""
        super()._add_com_thread()
        self._com_thread_pool["metric_sender"] = threading.Thread(target=self._publish_cmdp, daemon=True)
        self.log_cmdp_s.debug("Metric sender thread prepared and added to the pool.")

    def _publish_cmdp(self) -> None:
        """CMDP publishing loop sending metric data and updating subscriptions."""
        last_update: dict[str, datetime] = {}
        while self._com_thread_evt and not self._com_thread_evt.is_set():
            # run loop at intervals
            time.sleep(0.1)

            # update list of subscribers (both log and metric)
            try:
                self._cmdp_publisher.update_subscriptions()
            except ValueError as exc:
                self.log_cmdp_s.warning("Encountered unexpected subscription request: %s", exc)

            for metric_name, metric in self._metrics.items():
                # is it a scheduled metric?
                if not isinstance(metric, ScheduledMetric):
                    continue
                metric = cast(ScheduledMetric, metric)
                # do we have subscribers?
                if not self._cmdp_publisher.has_metric_subscribers(metric_name):
                    continue
                # are we in a correct state?
                if hasattr(self, "fsm"):
                    state = getattr(self, "fsm").state  # noqa: B009
                    if metric.allowed_states is not None and state not in metric.allowed_states:
                        self.log_cmdp_s.trace(f"Not sending metric {metric_name}: not allowed in {state}")
                        continue
                # is it time to update?
                update = False
                try:
                    last = last_update[metric_name]
                    if (datetime.now() - last).total_seconds() > metric.interval:
                        update = True
                except KeyError:
                    update = True
                if update:
                    try:
                        # Do not send if None type (e.g. currently unavailable)
                        value = metric.value_cb()
                        if value is not None:
                            self.log_cmdp_s.trace(f"Sending metric {metric_name} with value {value}{metric.unit}")
                            self._cmdp_publisher.send_metric(metric, value)
                        else:
                            self.log_cmdp_s.trace(f"Not sending metric {metric_name}: currently None")
                    except Exception as e:
                        self.log_cmdp_s.critical(f"Could not get metric {metric_name}: {repr(e)}")
                    last_update[metric_name] = datetime.now()

        self.log_cmdp_s.info("Monitoring metrics thread shutting down.")

    def reentry(self) -> None:
        """Orderly shut down monitoring communication infrastructure."""
        # remove all ZMQ log handlers
        self.log_cmdp_s.debug("Shutting down ZMQ logging.")
        # add zmq logging to existing Constellation loggers
        for _name, logger in logging.root.manager.loggerDict.items():
            if isinstance(logger, ConstellationLogger):
                if self._zmq_log_handler in logger.handlers:
                    logger.removeHandler(self._zmq_log_handler)
        # pass tear-down on to base class
        super().reentry()
        # Close sockets
        self._zmq_log_handler.close()
