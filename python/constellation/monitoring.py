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
        # ROOT logger needs to have a level set (initializes with level=NOSET)
        # The root level should be the lowest level that we want to see on any
        # handler, even streamed via ZMQ.
        logger = logging.getLogger()
        logger.setLevel("DEBUG")
        # NOTE: Logger object is a singleton and setup is only necessary once
        # for the given name.
        self._logger = logging.getLogger(name)
        self._zmqhandler = ZeroMQSocketLogHandler(self._transmitter)
        self._logger.addHandler(self._zmqhandler)

    def send_stat(self, metric: Metric):
        """Send a metric via ZMQ."""
        return self._transmitter.send_metric(metric)

    def close(self):
        """Close the ZMQ socket."""
        self._logger.removeHandler(self._zmqhandler)
        self._socket.close()


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
