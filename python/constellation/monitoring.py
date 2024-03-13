"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0
"""

import time
import logging
import zmq
from logging.handlers import QueueHandler, QueueListener
from .cmdp import CMDPTransmitter, Metric


class MonitoringManager:
    """Class managing the Constellation Monitoring Distribution Protocol."""

    def __init__(self, name: str, context: zmq.Context, port: int):
        """Set up logging and metrics transmitters."""
        # Create socket and bind wildcard
        self._socket = context.socket(zmq.PUB)
        self._socket.bind(f"tcp://*:{port}")
        self._transmitter = CMDPTransmitter(name, self._socket)
        # NOTE: Logger object is a singleton and setup is only necessary once
        # for the given name.
        self._logger = logging.getLogger(name)
        self._zmqhandler = ZeroMQSocketHandler(self._transmitter)
        self._logger.addHandler(self._zmqhandler)

    def send_stat(self, metric: Metric):
        """Send a metric via ZMQ."""
        return self._transmitter.send_metric(metric)

    def close(self):
        """Close the ZMQ socket."""
        self._logger.removeHandler(self._zmqhandler)
        self._socket.close()


class ZeroMQSocketHandler(QueueHandler):
    """This handler sends records to a ZMQ socket."""

    def __init__(self, transmitter: CMDPTransmitter):
        super().__init__(transmitter)

    def enqueue(self, record):
        self.queue.send(record)

    def close(self):
        if not self.queue.closed:
            self.queue.close()


class ZeroMQSocketListener(QueueListener):
    def __init__(self, uri, /, *handlers, **kwargs):
        self.ctx = kwargs.get("ctx") or zmq.Context()
        socket = zmq.Socket(self.ctx, zmq.SUB)
        # TODO implement a filter parameter to customize what to subscribe to
        socket.setsockopt_string(zmq.SUBSCRIBE, "LOG/")  # subscribe to LOGs
        socket.connect(uri)
        kwargs.pop("ctx", None)
        super().__init__(CMDPTransmitter(__name__, socket), *handlers, **kwargs)

    def dequeue(self, block):
        return self.queue.recv()


def main(args=None):
    """Start a simple log listener service."""
    import argparse

    parser = argparse.ArgumentParser(description=main.__doc__)
    parser.add_argument("--log-level", default="info")
    parser.add_argument("host", type=str)
    parser.add_argument("port", type=int)
    args = parser.parse_args(args)
    logger = logging.getLogger(__name__)
    formatter = logging.Formatter(
        "%(asctime)s | %(name)s |  %(levelname)s: %(message)s"
    )
    stream_handler = logging.StreamHandler()
    stream_handler.setLevel(args.log_level.upper())
    stream_handler.setFormatter(formatter)
    logger.addHandler(stream_handler)

    ctx = zmq.Context()
    zmqlistener = ZeroMQSocketListener(
        f"tcp://{args.host}:{args.port}", stream_handler, ctx=ctx
    )
    zmqlistener.start()
    while True:
        time.sleep(0.01)


if __name__ == "__main__":
    main()
