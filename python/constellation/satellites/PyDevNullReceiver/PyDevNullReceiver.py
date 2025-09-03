"""
SPDX-FileCopyrightText: 2025 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2
"""

import time
from typing import Any

from constellation.core.commandmanager import cscp_requestable
from constellation.core.message.cdtp2 import DataRecord
from constellation.core.message.cscp1 import CSCP1Message
from constellation.core.receiver_satellite import ReceiverSatellite


class PyDevNullReceiver(ReceiverSatellite):
    def __init__(self, *args: Any, **kwargs: Any) -> None:
        super().__init__(*args, **kwargs)
        self.start_time = time.time()
        self.data_rate = 0.0

    def do_starting(self, run_identifier: str) -> None:
        self.start_time = time.time()

    def do_stopping(self) -> None:
        stop_time = time.time()

        run_duration = stop_time - self.start_time

        assert self._drc is not None
        gigabyte_received = 1e-9 * self._drc.bytes_received
        self.data_rate = 8 * gigabyte_received / run_duration

        self.log.status(f"Received {gigabyte_received :.2g} GB in {run_duration :.0f}s ({self.data_rate :.3g} Gbps)")

    @cscp_requestable
    def get_data_rate(self, request: CSCP1Message) -> tuple[str, Any, dict[str, Any]]:
        return f"{self.data_rate:.3g} Gbps", self.data_rate, {}

    def receive_bor(self, sender: str, user_tags: dict[str, Any], configuration: dict[str, Any]) -> None:
        pass

    def receive_data(self, sender: str, data_record: DataRecord) -> None:
        pass

    def receive_eor(self, sender: str, user_tags: dict[str, Any], run_metadata: dict[str, Any]) -> None:
        pass
