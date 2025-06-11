"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2

A base module for a Constellation Satellite that sends data.
"""

import logging
import random
import threading
import time
from queue import Empty, Queue
from typing import Any

import numpy as np
import zmq

from .base import EPILOG
from .broadcastmanager import CHIRPServiceIdentifier
from .cdtp import CDTPMessageIdentifier, DataTransmitter
from .configuration import Configuration
from .error import debug_log, handle_error
from .logging import setup_cli_logging
from .satellite import Satellite, SatelliteArgumentParser


class PushThread(threading.Thread):
    """Thread that pushes CDTPMessages from a Queue to a ZMQ socket."""

    def __init__(
        self,
        name: str,
        stopevt: threading.Event,
        borevt: threading.Event,
        eorevt: threading.Event,
        socket: zmq.Socket,  # type: ignore[type-arg]
        timeouts: list[int],
        queue: Queue,  # type: ignore[type-arg]
        *args: Any,
        **kwargs: Any,
    ):
        """Initialize values.

        Arguments:
        - name       :: Name of the satellite.
        - stopevt    :: Event that if set lets the thread shut down.
        - borevt     :: Event that indicates when the BOR has been sent.
        - eorevt     :: Event that indicates when the EOR has been sent.
        - socket     :: The ZMQ socket to use.
        - timeouts   :: A list of timeout values for BOR, data, and EOR.
        - queue      :: The Queue to process payload and meta of data runs from.
        - context    :: ZMQ context to use (optional).
        """
        super().__init__(*args, **kwargs)
        self.name = name
        self._logger = logging.getLogger(__name__)
        self.stopevt = stopevt
        self.borevt = borevt
        self.eorevt = eorevt
        self.queue = queue
        self._socket = socket
        self._timeouts = timeouts

    def run(self) -> None:
        """Start sending data."""
        tm = DataTransmitter(self.name, self._socket)
        tm.BOR_timeout, tm.data_timeout, tm.EOR_timeout = self._timeouts

        while not self.stopevt.is_set():
            try:
                # blocking call but with timeout to prevent deadlocks
                data, meta = self.queue.get(block=True, timeout=0.5)
                # if we have data, send it
                if meta == CDTPMessageIdentifier.BOR:
                    tm.send_start(payload=data["payload"], meta=data["meta"])
                    self.borevt.set()
                elif meta == CDTPMessageIdentifier.EOR:
                    tm.send_end(payload=data["payload"], meta=data["meta"])
                    self.eorevt.set()
                else:
                    tm.send_data(payload=data, meta=meta)
                self._logger.debug(f"Sending packet number {tm.sequence_number}")
                self.queue.task_done()
            except Empty:
                # nothing to process
                pass

    def join(self, *args: Any, **kwargs: Any) -> Any:
        return super().join(*args, **kwargs)


class DataSender(Satellite):
    """Constellation Satellite which pushes data via ZMQ.

    You can modify the timeouts for packets sent via ZMQ depending on the type of packet:
    - `bor_timeout` (int) : timeout for beginning-of-run packets (in milliseconds).
    - `data_timeout` (int) : timeout for data packets (in milliseconds).
    - `eor_timeout` (int) : timeout for end-of-run packets (in milliseconds).

    A value of -1 is interpreted as infinite.
    """

    def __init__(self, *args: Any, data_port: int, **kwargs: Any):
        # initialize local attributes first:
        # beginning and end-of-run events: payloads and meta information
        self._beg_of_run: dict[str, dict[str, Any]] = {"payload": {}, "meta": {}}
        self._end_of_run: dict[str, dict[str, Any]] = {"payload": {}, "meta": {}}
        # set up the data pusher which will transmit data placed into the queue
        # via ZMQ socket
        self.data_queue: Queue = Queue()  # type: ignore[type-arg]
        self.data_port = data_port

        self._stop_pusher: threading.Event | None = None
        self._bor_sent: threading.Event | None = None
        self._eor_sent: threading.Event | None = None
        self._push_thread: threading.Thread | None = None

        self._bor_timeout: int = 10
        self._data_timeout: int = 10
        self._eor_timeout: int = 10

        # initialize satellite
        super().__init__(*args, **kwargs)

        self.log_cdtp_s = self.get_logger("CDTP")

        ctx = self.context or zmq.Context()
        self.socket = ctx.socket(zmq.PUSH)

        if not self.data_port:
            self.data_port = self.socket.bind_to_random_port("tcp://*")
        else:
            self.socket.bind(f"tcp://*:{self.data_port}")

        # run CHIRP
        self.register_offer(CHIRPServiceIdentifier.DATA, self.data_port)
        self.broadcast_offers()

    def reentry(self) -> None:
        # close the socket
        self.socket.close()
        self.log_cdtp_s.debug("Closed data socket")
        super().reentry()

    @property
    def EOR(self) -> Any:
        """Get optional payload for the end-of-run event (EOR)."""
        return self._end_of_run["payload"]

    @EOR.setter
    def EOR(self, payload: Any) -> None:
        """Set optional payload for the end-of-run event (EOR)."""
        self._end_of_run["payload"] = payload

    @property
    def BOR(self) -> Any:
        """Get optional payload for the beginning-of-run event (BOR)."""
        return self._beg_of_run["payload"]

    @BOR.setter
    def BOR(self, payload: Any) -> None:
        """Set optional payload for the beginning-of-run event (BOR)."""
        self._beg_of_run["payload"] = payload

    def _pre_initializing_hook(self, config: Configuration) -> None:
        """Configure values specific for all DataSender-type classes."""
        self._bor_timeout = self.config.setdefault("bor_timeout", 10)
        self._data_timeout = self.config.setdefault("data_timeout", 10)
        self._eor_timeout = self.config.setdefault("eor_timeout", 10)

    @handle_error
    @debug_log
    def _wrap_launch(self, payload: Any) -> str:
        """Wrapper for the 'launching' transitional state of the FSM.

        This method starts the PushThread for the DataSender.

        """
        self._stop_pusher = threading.Event()
        self._bor_sent = threading.Event()
        self._eor_sent = threading.Event()
        self._push_thread = PushThread(
            name=self.name,
            stopevt=self._stop_pusher,
            borevt=self._bor_sent,
            eorevt=self._eor_sent,
            socket=self.socket,
            timeouts=[self._bor_timeout, self._data_timeout, self._eor_timeout],
            queue=self.data_queue,
            daemon=True,  # terminate with the main thread
        )
        # self._push_thread.name = f"{self.name}_Pusher-thread"
        self._push_thread.start()
        self.log_cdtp_s.info(f"Satellite {self.name} publishing data on port {self.data_port}")
        res: str = super()._wrap_launch(payload)
        return res

    @handle_error
    @debug_log
    def _wrap_land(self, payload: Any) -> str:
        """Wrapper for the 'landing' transitional state of the FSM.

        This method will stop the PushThread.

        """
        # satisfy mypy and verify out setup:
        if not self._stop_pusher or not self._push_thread:
            raise RuntimeError("Data pusher thread not set up correctly")
        self._stop_pusher.set()
        try:
            self._push_thread.join(timeout=10)
        except TimeoutError:
            self.log_cdtp_s.warning("Unable to close push thread. Process timed out.")
        res: str = super()._wrap_land(payload)
        return res

    def _pre_run_hook(self, run_identifier: str) -> None:
        """Hook run immediately before do_run() is called.

        Configure and send the Beginning Of Run (BOR) message.

        """
        # Beginning of run message. If nothing was provided by the user (yet),
        # use the configuration dictionary as a payload
        if not self.BOR:
            self.BOR = self.config._config
        self.log_cdtp_s.debug("Sending BOR")
        self.data_queue.put((self._beg_of_run, CDTPMessageIdentifier.BOR))
        if self._bor_timeout < 0:
            timeout = None
        else:
            # convert to seconds
            timeout = self._bor_timeout / 1000
        # satisfy mypy and verify out setup:
        if not self._bor_sent:
            raise RuntimeError("Data pusher events not set up correctly")
        self._bor_sent.wait(timeout)
        if not self._bor_sent.is_set():
            raise RuntimeError("Timeout reached when sending BOR. No DataReceiver available?")

    @handle_error
    @debug_log
    def _wrap_stop(self, payload: Any) -> str:
        """Wrapper for the 'stopping' transitional state of the FSM.

        Sends the EOR event after base class wrapper and `do_stopping` have
        finished.

        """
        res: str = super()._wrap_stop(payload)
        self.log_cdtp_s.debug("Sending EOR")
        self.data_queue.put((self._end_of_run, CDTPMessageIdentifier.EOR))
        if self._eor_timeout < 0:
            timeout = None
        else:
            # convert to seconds
            timeout = self._eor_timeout / 1000
        assert isinstance(self._eor_sent, threading.Event)
        self._eor_sent.wait(timeout)
        if not self._eor_sent.is_set():
            raise RuntimeError("Timeout reached when sending EOR. No DataReceiver available?")
        return res

    def do_run(self, run_identifier: str) -> str:
        """Perform the data acquisition and enqueue the results.

        This is only an abstract method. Inheriting classes must implement their
        own acquisition method.

        This method will be executed in a separate thread by the underlying
        Satellite class. It therefore needs to monitor the self.stop_running
        Event and close itself down if the Event is set.

        If you want to transmit a payload as part of the end-of-run event (BOR),
        set the corresponding value via the `BOR` property before leaving this
        method.

        This method should return a string that will be used for setting the
        Status once the data acquisition is finished.

        """
        raise NotImplementedError


class RandomDataSender(DataSender):
    """Constellation Satellite which pushes RANDOM data via ZMQ."""

    def do_run(self, run_identifier: str) -> str:
        """Example implementation that generates random values."""
        samples = np.linspace(0, 2 * np.pi, 1024, endpoint=False)
        fs = random.uniform(0, 3)
        data_load = np.sin(2 * np.pi * fs * samples)

        t0 = time.time_ns()

        num = 0
        # assert for mypy static type analysis
        assert isinstance(self._state_thread_evt, threading.Event)

        while not self._state_thread_evt.is_set():
            self.data_queue.put((data_load.tobytes(), {"dtype": f"{data_load.dtype}"}))
            self.log_cdtp_s.debug(f"Queueing data packet {num}")
            num += 1
            time.sleep(0.5)

        t1 = time.time_ns()
        self.log_cdtp_s.info(f"total time for {num} evt / {num * len(data_load) / 1024 / 1024}MB: {(t1 - t0) / 1000000000}s")
        return "Finished acquisition"


# -------------------------------------------------------------------------


class DataSenderArgumentParser(SatelliteArgumentParser):
    """Customized Argument parser providing DataSender-specific options."""

    def __init__(self, *args: Any, **kwargs: Any):
        super().__init__(*args, **kwargs)
        self.network.add_argument(
            "--data-port",
            "--cdtp-port",
            type=int,
            help="The port for sending data via the " "Constellation Data Transfer Protocol (default: %(default)s).",
        )


def main(args: Any = None) -> None:
    """Start the RandomDataSender demonstration satellite.

    This Satellite sends random data via CDTP and can be used to test the
    protocol.

    """
    parser = DataSenderArgumentParser(description=main.__doc__, epilog=EPILOG)
    args = vars(parser.parse_args(args))

    # Set up logging
    setup_cli_logging(args.pop("level"))

    # start server with remaining args
    s = RandomDataSender(**args)
    s.run_satellite()


if __name__ == "__main__":
    main()
