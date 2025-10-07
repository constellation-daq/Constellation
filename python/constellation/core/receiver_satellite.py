"""
SPDX-FileCopyrightText: 2025 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2

A base module for a Constellation Satellite that receives data.
"""

import time
from typing import Any

from .cdtp import DataReceiver, RecvTimeoutError
from .chirp import CHIRPServiceIdentifier
from .chirpmanager import DiscoveredService, chirp_callback
from .cmdp import MetricsType
from .configuration import Configuration
from .error import debug_log, handle_error
from .message.cdtp2 import DataRecord
from .monitoring import schedule_metric
from .satellite import Satellite


class ReceiverSatellite(Satellite):
    """Constellation Satellite which can receive data via CDTP"""

    def __init__(self, *args: Any, **kwargs: Any):
        self._drc: DataReceiver | None = None

        # Initialize satellite
        super().__init__(*args, **kwargs)

        self.log_cdtp = self.get_logger("DATA")

    def _pre_initializing_hook(self, config: Configuration) -> None:
        """Configure values specific for all ReceiverSatellite-type classes."""
        super()._pre_initializing_hook(config)
        data_transmitters = self.config.setdefault("_data_transmitters", None)
        if data_transmitters:
            if not isinstance(data_transmitters, list) or not all([isinstance(t, str) for t in data_transmitters]):
                msg = "The '_data_transmitters' configuration parameter needs to be a list of strings "
                msg += '(e.g. \'["TachyonDetector.array1", "XRayCamera.cam1"]\')'
                raise ValueError(msg)
            data_transmitters = set(data_transmitters)
        # Create data receiver
        self._drc = DataReceiver(
            self.context, self.log_cdtp, self.receive_bor, self.receive_data, self.receive_eor, data_transmitters
        )
        # EOR timeout
        self._drc.eor_timeout = self.config.setdefault("_eor_timeout", 10)
        self.log_cdtp.debug("Timeout for EOR messages is %ss", self._drc.eor_timeout)

    def _pre_run_hook(self, run_identifier: str) -> None:
        """Hook run immediately before `do_run()` is called.

        Start the data receiver and requests DATA services via CHIRP.
        """
        # Start data receiver
        assert self._drc is not None
        self._drc.start_receiving()
        # Request data services
        self.request(CHIRPServiceIdentifier.DATA)
        # Add data services already discovered
        for service in self.get_discovered(CHIRPServiceIdentifier.DATA):
            self._drc.add_sender(service)

    @handle_error
    @debug_log
    def _wrap_stop(self, payload: Any) -> Any:
        # Stop data receiver
        assert self._drc is not None
        self._drc.stop_receiving()

        return super()._wrap_stop(payload)

    @handle_error
    @debug_log
    def _wrap_interrupt(self, payload: Any) -> str:
        # Stop data receiver but do not throw if not all EOR messages received
        assert self._drc is not None
        if self._drc.running:
            try:
                self._drc.stop_receiving()
            except RecvTimeoutError as e:
                self.log_cdtp.warning(str(e))

        res: str = super()._wrap_interrupt(payload)
        return res

    @handle_error
    @debug_log
    def _wrap_failure(self, payload: Any) -> str:
        # Stop data receiver pull thread directly
        if self._drc is not None and self._drc.running:
            self._drc._stop_pull_thread()

        res: str = super()._wrap_failure(payload)
        return res

    @chirp_callback(CHIRPServiceIdentifier.DATA)
    def _add_sender_callback(self, service: DiscoveredService) -> None:
        """Callback method for connecting to data service."""
        if self._drc is not None:
            if not service.alive:
                self._drc.remove_sender(service)
            else:
                self._drc.add_sender(service)

    @schedule_metric("B", MetricsType.LAST_VALUE, 10)
    def rx_bytes(self) -> int | None:
        if self._drc is not None and self._drc.running:
            return self._drc.bytes_received
        return None

    @property
    def data_transmitters(self) -> set[str] | None:
        if self._drc is not None:
            return self._drc.data_transmitters
        return None

    def receive_bor(self, sender: str, user_tags: dict[str, Any], configuration: dict[str, Any]) -> None:
        """Receive begin-of-run (BOR)"""
        raise NotImplementedError()

    def receive_data(self, sender: str, data_record: DataRecord) -> None:
        """Receive data"""
        raise NotImplementedError()

    def receive_eor(self, sender: str, user_tags: dict[str, Any], run_metadata: dict[str, Any]) -> None:
        """Receive end-of-run (EOR)"""
        raise NotImplementedError()

    def do_run(self, run_identifier: str) -> str:
        """Run loop

        NOTE: This must not be overridden by receiver satellite implementations!
        """
        assert self._drc is not None
        assert self._state_thread_evt is not None
        while not self._state_thread_evt.is_set():
            # Check and rethrow exception from BasePool
            self._drc.check_exception()
            # Wait a bit to avoid hot loop
            time.sleep(0.1)
        return "Finished run"
