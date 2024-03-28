"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0
"""

import time
import logging
import zmq
import threading
from functools import wraps
from datetime import datetime
from logging.handlers import QueueHandler, QueueListener
from .cmdp import CMDPTransmitter, Metric, MetricsType
from .base import BaseSatelliteFrame


def schedule_metric(handling: MetricsType, interval: float):
    """Schedule a function for callback at interval [s] and send Metric.

    The function should take no arguments and return a value [any] and a unit
    [str].

    """

    def decorator(func):
        @wraps(func)
        def wrapper(*args, **kwargs):
            val, unit = func(*args, **kwargs)
            m = Metric(
                name=func.__name__,
                description=func.__doc__,
                unit=unit,
                handling=handling,
                value=val,
            )
            return m

        # mark function as chirp callback
        wrapper.metric_scheduled = interval
        return wrapper

    return decorator


def get_scheduled_metrics(cls):
    """Loop over all class methods and return those marked as metric."""
    res = {}
    for func in dir(cls):
        call = getattr(cls, func)
        if callable(call) and not func.startswith("__"):
            # regular method
            if hasattr(call, "metric_scheduled"):
                val = getattr(call, "metric_scheduled")
                if isinstance(val, float):
                    # safeguard for tests only: a mock context would end up here
                    res[call.__name__] = {"function": call, "interval": val}
    return res


class MonitoringSender(BaseSatelliteFrame):
    """Class managing the Constellation Monitoring Distribution Protocol."""

    def __init__(self, name: str, mon_port: int, interface: str, **kwds):
        """Set up logging and metrics transmitters."""
        super().__init__(name=name, interface=interface, **kwds)

        # Create monitoring socket and bind wildcard
        socket = self.context.socket(zmq.PUB)
        socket.bind(f"tcp://{interface}:{mon_port}")
        self._mon_tm = CMDPTransmitter(name, socket)

        # Set up ZMQ logging
        # ROOT logger needs to have a level set (initializes with level=NOSET)
        # The root level should be the lowest level that we want to see on any
        # handler, even streamed via ZMQ.
        logger = logging.getLogger()
        logger.setLevel("DEBUG")
        # NOTE: Logger object is a singleton and setup is only necessary once
        # for the given name.
        self._zmq_log_handler = ZeroMQSocketLogHandler(self._mon_tm)
        self.log.addHandler(self._zmq_log_handler)

        # dict to keep scheduled intervals for fcn polling
        self._metrics_callbacks = get_scheduled_metrics(self)

    def send_metric(self, metric: Metric):
        """Send a metric via ZMQ."""
        return self._mon_tm.send_metric(metric)

    def _add_com_thread(self):
        """Add the metric sender thread to the communication thread pool."""
        super()._add_com_thread()
        self._com_thread_pool["metric_sender"] = threading.Thread(
            target=self._send_metrics, daemon=True
        )
        self.log.debug("Metric sender thread prepared and added to the pool.")

    def _send_metrics(self):
        """Metrics sender loop."""
        last_update = {}
        while not self._com_thread_evt.is_set():
            for metric, param in self._metrics_callbacks.items():
                update = False
                try:
                    last = last_update[metric]
                    if (datetime.now() - last).total_seconds() > param["interval"]:
                        update = True
                except KeyError:
                    update = True
                if update:
                    try:
                        self.send_metric(param["function"]())
                    except Exception as e:
                        self.log.error(
                            "Could not retrieve metric %s: %s", metric, repr(e)
                        )
                    last_update[metric] = datetime.now()

            time.sleep(0.1)
        # clean up
        self.close()

    def close(self):
        """Close the ZMQ socket."""
        self.log.removeHandler(self._zmq_log_handler)
        self._mon_tm.close()


class ZeroMQSocketLogHandler(QueueHandler):
    """This handler sends records to a ZMQ socket."""

    def __init__(self, transmitter: CMDPTransmitter):
        super().__init__(transmitter)

    def enqueue(self, record):
        self.queue.send(record)

    def close(self):
        if not self.queue.closed:
            self.queue.close()


class ZeroMQSocketLogListener(QueueListener):
    def __init__(self, uri, /, *handlers, **kwargs):
        context = kwargs.get("ctx") or zmq.Context()
        self.socket = context.socket(zmq.SUB)
        # TODO implement a filter parameter to customize what to subscribe to
        self.socket.setsockopt_string(zmq.SUBSCRIBE, "LOG/")  # subscribe to LOGs
        self.socket.connect(uri)
        kwargs.pop("ctx", None)
        super().__init__(CMDPTransmitter(__name__, self.socket), *handlers, **kwargs)

    def dequeue(self, block):
        return self.queue.recv()

    def stop(self):
        """Close socket and stop thread."""
        # stop thread
        super().stop()
        self.socket.close()


def main(args=None):
    """Start a simple log listener service."""
    import argparse
    import coloredlogs

    parser = argparse.ArgumentParser(description=main.__doc__)
    parser.add_argument("--log-level", default="info")
    parser.add_argument("host", type=str)
    parser.add_argument("port", type=int)
    args = parser.parse_args(args)
    logger = logging.getLogger(__name__)
    # set up logging
    coloredlogs.install(level=args.log_level.upper(), logger=logger)

    ctx = zmq.Context()
    zmqlistener = ZeroMQSocketLogListener(
        f"tcp://{args.host}:{args.port}", logger.handlers[0], ctx=ctx
    )
    zmqlistener.start()
    while True:
        time.sleep(0.01)


if __name__ == "__main__":
    main()
