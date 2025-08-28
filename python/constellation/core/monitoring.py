"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2
"""

import logging
import os
import pathlib
import threading
import time
from collections.abc import Callable
from datetime import datetime
from functools import wraps
from logging.handlers import QueueListener
from queue import Empty, Queue
from typing import Any, ParamSpec, TypeVar, cast

import zmq

from .base import EPILOG, BaseSatelliteFrame, ConstellationArgumentParser
from .chirp import CHIRPServiceIdentifier
from .chirpmanager import CHIRPManager, DiscoveredService, chirp_callback
from .cmdp import CMDPPublisher, CMDPTransmitter, Metric, MetricsType, decode_metric
from .logging import ConstellationLogger, ZeroMQSocketLogHandler, setup_cli_logging

P = ParamSpec("P")
B = TypeVar("B", bound=BaseSatelliteFrame)


def schedule_metric(unit: str, handling: MetricsType, interval: float) -> Callable[[Callable[P, Any]], Callable[P, Metric]]:
    """Schedule a function for callback at interval [s] and send Metric.

    The function should take no arguments and return a value [any]
    """

    def decorator(func: Callable[P, Any]) -> Callable[P, Metric]:
        @wraps(func)
        def wrapper(*args: P.args, **kwargs: P.kwargs) -> Metric:
            val = func(*args, **kwargs)
            return Metric(
                name=func.__name__,
                unit=unit,
                handling=handling,
                value=val,
            )

        # mark function as chirp callback
        wrapper.metric_scheduled = interval  # type: ignore[attr-defined]
        return wrapper

    return decorator


def get_scheduled_metrics(cls: object) -> dict[str, dict[str, Any]]:
    """Loop over all class methods and return those marked as metric."""
    res = {}
    for func in dir(cls):
        call = getattr(cls, func)
        if callable(call) and not func.startswith("__"):
            # regular method
            if hasattr(call, "metric_scheduled") and hasattr(call, "__name__"):
                res[call.__name__] = {"function": call, "interval": getattr(call, "metric_scheduled")}
    return res


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

        # dict to keep scheduled intervals for fcn polling
        self._metrics_callbacks = get_scheduled_metrics(self)

        # Open ZMQ socket using (X)PUB/SUB pattern. XPUB allows us to handle
        # subscription messages directly.
        cmdp_socket = self.context.socket(zmq.XPUB)
        if not mon_port:
            self.mon_port = cmdp_socket.bind_to_random_port("tcp://*")
        else:
            cmdp_socket.bind(f"tcp://*:{mon_port}")
            self.mon_port = mon_port
        # Instantiate CMDP publish transmitter and CMDP log handler
        self._cmdp_transmitter = CMDPPublisher(self.name, cmdp_socket)
        self._zmq_log_handler = ZeroMQSocketLogHandler(self._cmdp_transmitter)

        # register all metrics callbacks for CMDP notifications
        for name, details in self._metrics_callbacks.items():
            self._cmdp_transmitter.register_stat(name, details["function"].__doc__)

        self._zmq_log_handler.setLevel("TRACE")
        # add zmq logging to existing Constellation loggers
        for name, logger in logging.root.manager.loggerDict.items():
            # only configure Constellation's own loggers
            if isinstance(logger, ConstellationLogger):
                # only configure logger if we keep a reference to it ourselves
                if logger in self.__dict__.values():
                    self._configure_cmdp_logger(logger)
        # update list of subscribers (both log and metric)
        self._cmdp_transmitter.update_subscriptions()

    def _configure_cmdp_logger(self, logger: ConstellationLogger) -> None:
        """Configure log handler for CMDP messaging via ZMQ."""
        if self._zmq_log_handler not in logger.handlers:
            logger.addHandler(self._zmq_log_handler)
        self._cmdp_transmitter.register_log(logger.name, "")

    def schedule_metric(
        self,
        name: str,
        unit: str,
        handling: MetricsType,
        interval: float,
        callback: Callable[..., Any],
    ) -> None:
        """Schedule a callback at regular intervals.

        The callable needs to return a value [any] and a unit [str] and take no
        arguments. If you have a callable that requires arguments, consider
        using `functools.partial` to fill in the necessary information at
        scheduling time.

        """

        def wrapper() -> Metric:
            val = callback()
            return Metric(
                name=name,
                unit=unit,
                handling=handling,
                value=val,
            )

        self._metrics_callbacks[name] = {"function": wrapper, "interval": interval}
        self._cmdp_transmitter.register_stat(name, callback.__doc__)

    def send_metric(self, metric: Metric) -> None:
        """Send a single metric via ZMQ."""
        self._cmdp_transmitter.send_metric(metric)

    def reset_scheduled_metrics(self) -> None:
        """Reset all previously scheduled metrics.

        Will only schedule metrics provided via decorator.

        """
        self._metrics_callbacks = get_scheduled_metrics(self)

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
            self._cmdp_transmitter.update_subscriptions()

            # if the satellite is not ready for sending metrics then skip ahead
            # and try again later
            readyfcn = getattr(self, "_is_sending_metrics", None)
            if readyfcn and not readyfcn():
                continue

            for metric_name, param in self._metrics_callbacks.items():
                # do we have subscribers?
                if not self._cmdp_transmitter.has_metric_subscribers(metric_name):
                    continue
                # is it time to update?
                update = False
                try:
                    last = last_update[metric_name]
                    if (datetime.now() - last).total_seconds() > param["interval"]:
                        update = True
                except KeyError:
                    update = True
                if update:
                    try:
                        # Do not send if None type (e.g. currently unavailable)
                        metric = param["function"]()
                        if metric.value is not None:
                            self.send_metric(metric)
                        else:
                            self.log_cmdp_s.trace(f"Not sending metric {metric_name}: currently None")
                    except Exception as e:
                        self.log_cmdp_s.error(f"Could not retrieve metric {metric_name}: {repr(e)}")
                    last_update[metric_name] = datetime.now()

        self.log_cmdp_s.info("Monitoring metrics thread shutting down.")

    def reentry(self) -> None:
        """Orderly shut down monitoring communication infrastructure."""
        # remove all ZMQ log handlers
        self.log_cmdp_s.debug("Shutting down ZMQ logging.")
        # add zmq logging to existing Constellation loggers
        for name, logger in logging.root.manager.loggerDict.items():
            if isinstance(logger, ConstellationLogger):
                if self._zmq_log_handler in logger.handlers:
                    logger.removeHandler(self._zmq_log_handler)
        # pass tear-down on to base class
        super().reentry()
        # Close sockets
        self._zmq_log_handler.close()


class ZeroMQSocketLogListener(QueueListener):
    """This listener receives messages from a CMDPTransmitter.

    NOTE that the corresponding socket should only subscribe to LOG messages!

    """

    def __init__(self, transmitter: CMDPTransmitter, /, *handlers: Any, **kwargs: Any):
        super().__init__(cast(Queue, transmitter), *handlers, **kwargs)  # type: ignore[type-arg]
        self._stop_recv = threading.Event()

    def dequeue(self, block: bool) -> logging.LogRecord:
        # FIXME it is quite likely that this blocking call causes errors when
        # shutting down as the ZMQ context is removed before this call ends.
        record = None
        while not record and not self._stop_recv.is_set():
            try:
                record = self.queue.recv()  # type: ignore[attr-defined]
            except zmq.ZMQError:
                pass
        if self._stop_recv.is_set():
            # close down
            return self._sentinel  # type: ignore[no-any-return, attr-defined]
        return cast(logging.LogRecord, record)

    def stop(self) -> None:
        """Close socket and stop thread."""
        super().stop()
        self.queue.close()  # type: ignore[attr-defined]

    def enqueue_sentinel(self) -> None:
        self._stop_recv.set()


class StatListener(CHIRPManager):
    """Simple listener class to receive metrics from a Constellation."""

    def __init__(self, name: str, group: str, interface: list[str] | None, **kwds: Any):
        """Initialize values.

        Arguments:
        - name ::  name of this Monitor
        - group ::  group of controller
        - interface :: the interfaces to connect to
        """
        super().__init__(name=name, group=group, interface=interface, **kwds)

        self.log_cmdp_l = self.get_logger("MNTR")

        # Set up the metric poller which will monitor all ZMQ metric subscription sockets
        self._metric_sockets: dict[str, zmq.Socket] = {}  # type: ignore[type-arg]
        self._metric_poller = zmq.Poller()
        self._metric_poller_lock = threading.Lock()

        self.request(CHIRPServiceIdentifier.MONITORING)

    def metric_callback(self, metric: Metric) -> None:
        """Metric callback."""
        self.log_cmdp_l.debug(f"Received metric {metric.name} from {metric.sender}: {metric.value} {metric.unit}")

    @chirp_callback(CHIRPServiceIdentifier.MONITORING)
    def _add_satellite_callback(self, service: DiscoveredService) -> None:
        """Callback method connecting to satellite."""
        if not service.alive:
            self._remove_satellite(service)
        else:
            self._add_satellite(service)

    def _add_satellite(self, service: DiscoveredService) -> None:
        address = "tcp://" + service.address + ":" + str(service.port)
        uuid = str(service.host_uuid)
        self.log_cmdp_l.debug("Connecting to %s, address %s...", uuid, address)

        # create socket for metrics
        socket = self.context.socket(zmq.SUB)
        socket.connect(address)
        socket.setsockopt_string(zmq.SUBSCRIBE, "STAT/")
        self._metric_sockets[uuid] = socket
        self._metric_poller.register(socket, zmq.POLLIN)

    def _remove_satellite(self, service: DiscoveredService) -> None:
        uuid = str(service.host_uuid)
        self.log_cmdp_l.debug("Departure of %s.", service.host_uuid)
        try:
            with self._metric_poller_lock:
                socket = self._metric_sockets.pop(uuid)
                self._metric_poller.unregister(socket)
                socket.close()
        except KeyError:
            pass

    def _add_com_thread(self) -> None:
        """Add the metric receiver thread to the communication thread pool."""
        super()._add_com_thread()
        self._com_thread_pool["metric_receiver"] = threading.Thread(target=self._receive_metrics, daemon=True)
        self.log_cmdp_l.debug("Metric receiver thread prepared and added to the pool.")

    def _receive_metrics(self) -> None:
        """Main loop to receive metrics."""
        # assert for mypy static type analysis
        assert isinstance(self._com_thread_evt, threading.Event), "Thread Event not set up correctly"

        while not self._com_thread_evt.is_set():
            with self._metric_poller_lock:
                sockets_ready = dict(self._metric_poller.poll(timeout=250))
                if sockets_ready:
                    for socket in sockets_ready.keys():
                        binmsg = socket.recv_multipart()
                        metric = decode_metric("", binmsg[0].decode("utf-8"), binmsg)
                        self.metric_callback(metric)
            # If no sockets are connected, the poller returns immediately -> sleep to prevent hot loop
            time.sleep(0.250)

    def _metrics_listening_shutdown(self) -> None:
        with self._metric_poller_lock:
            for _uuid, socket in self._metric_sockets.items():
                self._metric_poller.unregister(socket)
                socket.close()
            self._metric_sockets = {}

    def reentry(self) -> None:
        self._metrics_listening_shutdown()
        super().reentry()

    def run_listener(self) -> None:
        self._add_com_thread()
        self._start_com_threads()
        while self._com_thread_evt and not self._com_thread_evt.is_set():
            try:
                time.sleep(250e-3)
            except KeyboardInterrupt:
                self.log_cmdp_l.warning("Listener caught KeyboardInterrupt, shutting down.")
                break


class MonitoringListener(StatListener):
    """Simple monitor class to receive logs and metrics from a Constellation."""

    def __init__(self, *args, **kwargs):  # type: ignore[no-untyped-def]
        super().__init__(*args, **kwargs)

        self._log_listeners: dict[str, ZeroMQSocketLogListener] = {}

        # set up thread to handle incoming tasks (e.g. CHIRP discoveries)
        self._task_handler_event = threading.Event()
        self._task_handler_thread = threading.Thread(target=self._run_task_handler, daemon=True)
        self._task_handler_thread.start()

    def _add_satellite(self, service: DiscoveredService) -> None:
        # add for metrics
        super()._add_satellite(service)
        # create socket for logging
        address = "tcp://" + service.address + ":" + str(service.port)
        uuid = str(service.host_uuid)
        socket = self.context.socket(zmq.SUB)
        # add timeout to avoid deadlocks
        socket.setsockopt(zmq.RCVTIMEO, 250)
        # subscribe and create log listener
        socket.connect(address)
        socket.setsockopt_string(zmq.SUBSCRIBE, "LOG/")
        listener = ZeroMQSocketLogListener(
            CMDPTransmitter(self.name, socket),
            *self.log_cmdp_l.handlers,
            respect_handler_level=True,  # handlers can have different log lvls
        )
        self._log_listeners[uuid] = listener
        listener.start()

    def _remove_satellite(self, service: DiscoveredService) -> None:
        # remove for metrics
        super()._remove_satellite(service)
        # remove from logging
        uuid = str(service.host_uuid)
        try:
            listener = self._log_listeners.pop(uuid)
            listener.stop()
        except KeyError:
            pass

    def _run_task_handler(self) -> None:
        """Event loop for task handler-routine"""
        while self._task_handler_event and not self._task_handler_event.is_set():
            try:
                # blocking call but with timeout to prevent deadlocks
                task = self.task_queue.get(block=True, timeout=0.5)
                callback = task[0]
                args = task[1]
                try:
                    callback(*args)
                except Exception as e:
                    self.log_cmdp_l.exception(e)
            except Empty:
                # nothing to process
                pass

    def _log_listening_shutdown(self) -> None:
        for _uuid, listener in self._log_listeners.items():
            listener.stop()
        self._log_listeners = {}

    def reentry(self) -> None:
        self._log_listening_shutdown()
        super().reentry()


class FileMonitoringListener(MonitoringListener):
    def __init__(self, name: str, group: str, interface: list[str] | None, output_path: str):
        self.output_path = pathlib.Path(output_path)
        try:
            os.makedirs(self.output_path)
        except FileExistsError:
            pass
        try:
            os.mkdir(self.output_path / "logs")
            os.mkdir(self.output_path / "stats")
        except FileExistsError:
            pass

        super().__init__(name, group, interface)

        self._file_handler = logging.handlers.RotatingFileHandler(
            self.output_path / "logs" / (group + ".log"),
            maxBytes=10**7,
            backupCount=10,
        )
        formatter = logging.Formatter("%(asctime)s - %(name)s - %(levelname)s - %(message)s")
        self._file_handler.setFormatter(formatter)
        self._file_handler.setLevel(logging.DEBUG)
        self.log_cmdp_l.addHandler(self._file_handler)

    def metric_callback(self, metric: Metric) -> None:
        super().metric_callback(metric)
        fname = f"stats/{metric.sender}.{metric.name.lower()}.csv"
        path = self.output_path / fname
        ts = metric.time.to_unix()
        with open(path, "a") as csv:
            csv.write(f"{ts}, {metric.value}, '{metric.unit}'\n")

    def reentry(self) -> None:
        self.log_cmdp_l.removeHandler(self._file_handler)
        super().reentry()


def main(args: Any = None) -> None:
    """Start a simple log listener service."""
    parser = ConstellationArgumentParser(description=main.__doc__, epilog=EPILOG)
    parser.add_argument(
        "-o",
        "--output-path",
        type=str,
        help="The path to write log and metric data to.",
    )
    # get a dict of the parsed arguments
    args = vars(parser.parse_args(args))

    # Set up logging
    setup_cli_logging(args.pop("level"))

    mon: MonitoringListener

    # create output directories and configure file writer logger
    output_path: str | None = args.pop("output_path")
    if output_path is not None:
        mon = FileMonitoringListener(**args, output_path=output_path)
    else:
        mon = MonitoringListener(**args)

    mon.run_listener()


if __name__ == "__main__":
    main()
