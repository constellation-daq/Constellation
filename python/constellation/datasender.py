#!/usr/bin/env python3
"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0

A base module for a Constellation Satellite that sends data.
"""

import time
import datetime
import threading
import os
import logging
from typing import Optional
from queue import Queue, Empty

import zmq

from .cdtp import DataTransmitter, CDTPMessage
from .satellite import Satellite


class PushThread(threading.Thread):
    """Thread that pushes CDTPMessages from a Queue to a ZMQ socket."""

    def __init__(
        self,
        name: str,
        stopevt: threading.Event,
        port: int,
        queue: Queue,
        *args,
        context: Optional[zmq.Context] = None,
        **kwargs,
    ):
        """Initialize values.

        Arguments:
        - stopevt    :: Event that if set lets the thread shut down.
        - port       :: The port to bind to.
        - queue      :: The Queue to process CDTPMessages from.
        - context    :: ZMQ context to use (optional).
        """
        super().__init__(*args, **kwargs)
        self.name = name
        self._logger = logging.getLogger(__name__)
        self.stopevt = stopevt
        self.queue = queue
        ctx = context or zmq.Context()
        self._socket = ctx.socket(zmq.PUSH)
        self._socket.bind(f"tcp://*:{port}")

    def run(self):
        """Start sending data."""
        transmitter = DataTransmitter(self.name, self._socket)
        BOR = True
        while not self.stopevt.is_set():
            try:
                # blocking call but with timeout to prevent deadlocks
                payload = self.queue.get(block=True, timeout=0.5)
                # if we have data, send it
                if BOR:
                    transmitter.send_start(payload)
                    BOR = False
                else:
                    transmitter.send_data(payload)
                self._logger.debug(
                    f"Sending packet number {transmitter.sequence_number}"
                )
                self.queue.task_done()
            except Empty:
                # nothing to process
                pass
        transmitter.send_end(payload)


class DataSender(Satellite):
    """Constellation Satellite which pushes data via ZMQ."""

    def __init__(self, *args, data_port: int, **kwargs):
        super().__init__(*args, **kwargs)

        # set up the data pusher which will transmit
        # data placed into the queue via ZMQ socket
        self._stop_pusher = threading.Event()
        self.data_queue = Queue()
        self._push_thread = PushThread(
            name=self.name,
            stopevt=self._stop_pusher,
            port=data_port,
            queue=self.data_queue,
            context=self.context,
            daemon=True,  # terminate with the main thread
        )
        # self._push_thread.name = f"{self.name}_Pusher-thread"
        self._push_thread.start()
        self.log.info(f"Satellite {self.name} publishing data on port {data_port}")

    def do_run(self):
        """Perform the data acquisition and enqueue the results.

        This method will be executed in a separate thread by the underlying
        Satellite class. It therefore needs to monitor the self.stop_running
        Event and close itself down if the Event is set.

        This is only an abstract method. Inheriting classes must implement their
        own acquisition method.

        """
        raise NotImplementedError


class RandomDataSender(DataSender):
    """Constellation Satellite which pushes RANDOM data via ZMQ."""

    def do_run(self):
        """Example implementation that generates random values."""
        payload = os.urandom(1024)

        t0 = time.time()
        num = 0
        while not self._stop_running.is_set():
            meta = {"eventid": num, "time": datetime.datetime.now().isoformat()}
            data = CDTPMessage(payload, meta)
            self.data_queue.put(data)
            self.log.debug(f"Queueing data packet {num}")
            num += 1
            time.sleep(0.5)

        """
        # Testing shut down process
        meta = {"eventid": num, "time": datetime.datetime.now().isoformat(), "islast": True}
        data = CDTPMessage(payload, meta)
        self.data_queue.put(data)
        self.logger.debug(f"Queueing data packet {num}")
        num += 1
        time.sleep(0.5) """

        t1 = time.time()
        self.log.info(
            f"total time for {num} evt / {num * len(payload) / 1024 / 1024}MB: {t1 - t0}s"
        )


# -------------------------------------------------------------------------


def main(args=None):
    """Start the Lecroy oscilloscope device server."""
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument("--log-level", default="info")
    parser.add_argument("--cmd-port", type=int, default=23999)
    parser.add_argument("--log-port", type=int, default=55556)
    parser.add_argument("--hb-port", type=int, default=61234)
    parser.add_argument("--data-port", type=int, default=55557)

    args = parser.parse_args(args)
    logging.basicConfig(
        format="%(asctime)s %(levelname)s %(name)s: %(message)s",
        level=args.log_level.upper(),
    )

    # start server with remaining args
    s = RandomDataSender(
        "random_data_sender",
        cmd_port=args.cmd_port,
        hb_port=args.hb_port,
        log_port=args.log_port,
        data_port=args.data_port,
    )
    s.run_satellite()


if __name__ == "__main__":
    main()
