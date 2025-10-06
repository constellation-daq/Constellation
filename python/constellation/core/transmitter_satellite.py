"""
SPDX-FileCopyrightText: 2025 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2

A base module for a Constellation Satellite that sends data.
"""

from datetime import datetime, timezone
from typing import Any

import zmq

from . import __version__, __version_code_name__
from .cdtp import DataTransmitter, RunCondition, TransmitterState
from .chirpmanager import CHIRPServiceIdentifier
from .cmdp import MetricsType
from .configuration import Configuration
from .error import debug_log, handle_error
from .message.cdtp2 import DataRecord
from .monitoring import schedule_metric
from .satellite import Satellite, SatelliteArgumentParser


class TransmitterSatellite(Satellite):
    """Constellation Satellite which can send data via CDTP"""

    def __init__(self, data_port: int, *args: Any, **kwargs: Any):
        self._bor = dict[str, Any]()
        self._eor = dict[str, Any]()
        self._run_metadata = dict[str, Any]()
        self._mark_run_tainted = False

        # Initialize satellite
        super().__init__(*args, **kwargs)

        self.data_port = data_port
        self.log_cdtp = self.get_logger("DATA")

        # Create socket
        ctx = self.context or zmq.Context()
        self._cdtp_socket = ctx.socket(zmq.PUSH)
        if not self.data_port:
            self.data_port = self._cdtp_socket.bind_to_random_port("tcp://*")
        else:
            self._cdtp_socket.bind(f"tcp://*:{self.data_port}")
        self.log_cdtp.info(f"Publishing data on port {self.data_port}")

        # Create data transmitter
        self._dtm = DataTransmitter(self.name, self._cdtp_socket, self.log_cdtp, self._failure_callback)

        # Register CHIRP service
        self.register_offer(CHIRPServiceIdentifier.DATA, self.data_port)
        self.emit_offers()

    def reentry(self) -> None:
        # Close the socket
        self._cdtp_socket.close()
        self.log_cdtp.debug("Closed data socket")
        super().reentry()

    @property
    def eor(self) -> dict[str, Any]:
        """User tags for the end-of-run (EOR) message"""
        return self._eor

    @eor.setter
    def eor(self, user_tags: dict[str, Any]) -> None:
        self._eor = user_tags

    @property
    def bor(self) -> dict[str, Any]:
        """User tags for the begin-of-run (EOR) message"""
        return self._bor

    @bor.setter
    def bor(self, user_tags: dict[str, Any]) -> None:
        self._bor = user_tags

    def _pre_initializing_hook(self, config: Configuration) -> None:
        """Configure values specific for all TransmitterSatellite-type classes."""
        super()._pre_initializing_hook(config)
        self._dtm.bor_timeout = self.config.setdefault("_bor_timeout", 10)
        self._dtm.data_timeout = self.config.setdefault("_data_timeout", 10)
        self._dtm.eor_timeout = self.config.setdefault("_eor_timeout", 10)
        self._dtm.payload_threshold = self.config.setdefault("_payload_threshold", 128)
        self._dtm.queue_size = self.config.setdefault("_queue_size", 32768)
        self._data_license = self.config.setdefault("_data_license", "ODC-By-1.0")

    def _pre_run_hook(self, run_identifier: str) -> None:
        """Hook run immediately before `do_run()` is called.

        Send the BOR message and start the data transmitter.
        """
        # Run metadata
        self._run_metadata = {
            "version": __version__,
            "version_full": f"Constellation v{__version__} ({__version_code_name__})",
            "run_id": run_identifier,
            "time_start": datetime.now(timezone.utc),
            "license": self._data_license,
        }
        self._mark_run_tainted = False
        # Send BOR message
        self._dtm.send_bor(self._bor, self.config.get_applied())
        # Start data transmitter
        self._dtm.start_sending()

    @handle_error
    @debug_log
    def _wrap_stop(self, payload: Any) -> Any:
        """Wrapper for the 'stopping' transitional state of the FSM.

        Stops the data transmitter and sends the EOR message after `do_stopping()` has finished.
        """
        self._update_run_metadata(RunCondition.GOOD)

        res: str = super()._wrap_stop(payload)

        # Stop data transmitter
        self._dtm.stop_sending()
        # Send EOR message
        self._dtm.send_eor(self._eor, self._run_metadata)

        return res

    @handle_error
    @debug_log
    def _wrap_interrupt(self, payload: Any) -> str:
        """Wrapper for the 'interrupting' transitional state of the FSM.

        Stops the data transmitter and sends the EOR message after `do_interrupting()` has finished.
        """
        self._update_run_metadata(RunCondition.INTERRUPTED)

        res: str = super()._wrap_interrupt(payload)

        # Stop data transmitter
        self._dtm.stop_sending()
        # Send EOR message if BOR was sent
        if self._dtm.state == TransmitterState.BOR_RECEIVED:
            self._dtm.send_eor(self._eor, self._run_metadata)

        return res

    @handle_error
    @debug_log
    def _wrap_failure(self, payload: Any) -> str:
        """Wrapper for the 'interrupting' transitional state of the FSM.

        Stops the data transmitter and sends the EOR message after `do_failure()` has finished.
        """
        self.mark_run_tainted()
        self._update_run_metadata(RunCondition.ABORTED)

        res: str = super()._wrap_failure(payload)

        # Stop data transmitter
        self._dtm.stop_sending()
        # Send EOR message if BOR was sent
        if self._dtm.state == TransmitterState.BOR_RECEIVED:
            self._dtm.send_eor(self._eor, self._run_metadata)

        return res

    def _failure_callback(self, err_msg: str) -> None:
        """Failure callback for data transmitter"""
        self._transition("failure", err_msg, thread=False)
        self.log_cdtp.critical(err_msg)

    def _update_run_metadata(self, condition_code: RunCondition) -> None:
        """Update run metadata at the end of a run"""
        self._run_metadata["time_end"] = datetime.now(timezone.utc)
        if self._mark_run_tainted:
            condition_code |= RunCondition.TAINTED
        if self.run_degraded:
            condition_code |= RunCondition.DEGRADED
        self._run_metadata["condition_code"] = condition_code.value
        self._run_metadata["condition"] = condition_code.name

    @schedule_metric("B", MetricsType.LAST_VALUE, 10)
    def tx_bytes(self) -> int | None:
        if self._dtm is not None and self._dtm.state == TransmitterState.BOR_RECEIVED:
            return self._dtm.bytes_transmitted
        return None

    @schedule_metric("", MetricsType.LAST_VALUE, 10)
    def tx_records(self) -> int | None:
        if self._dtm is not None and self._dtm.state == TransmitterState.BOR_RECEIVED:
            return self._dtm.records_transmitted
        return None

    def can_send_record(self) -> bool:
        """Check if a data record can be send immediately"""
        return self._dtm.can_send_record()

    def new_data_record(self, tags: dict[str, Any] | None = None) -> DataRecord:
        """Return new data record for sending"""
        return self._dtm.new_data_record(tags)

    def send_data_record(self, data_record: DataRecord) -> None:
        """Queue a data record for sending"""
        self._dtm.send_data_record(data_record)

    def mark_run_tainted(self) -> None:
        """Mark the current run as tainted"""
        self._mark_run_tainted = True

    def do_run(self, run_identifier: str) -> str:
        """Perform the data acquisition and enqueue the results.

        This is only an abstract method. Inheriting classes must implement their
        own acquisition method.

        This method will be executed in a separate thread by the underlying
        Satellite class. It therefore needs to monitor the self.stop_running
        Event and close itself down if the Event is set.

        If you want to transmit a payload as part of the end-of-run message (EOR),
        set the corresponding value via the `eor` property before leaving this
        method.

        This method should return a string that will be used for setting the
        Status once the data acquisition is finished.

        """
        raise NotImplementedError


class TransmitterSatelliteArgumentParser(SatelliteArgumentParser):
    """Customized Argument parser providing DataSender-specific options."""

    def __init__(self, *args: Any, **kwargs: Any):
        super().__init__(*args, **kwargs)
        self.network.add_argument(
            "--data-port",
            "--cdtp-port",
            type=int,
            help="The port for sending data via the Constellation Data Transfer Protocol. "
            "A random port will be selected if none is specified.",
        )
