"""
SPDX-FileCopyrightText: 2025 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2
"""

import random
import time

from constellation.core.configuration import Configuration
from constellation.core.transmitter_satellite import TransmitterSatellite


class PyRandomTransmitter(TransmitterSatellite):
    def do_initializing(self, config: Configuration) -> None:
        self._block_size = config.setdefault("block_size", 1024)
        self._number_of_blocks = config.setdefault("number_of_blocks", 1)

    def do_launching(self) -> None:
        self._blocks = []
        for _ in range(self._number_of_blocks):
            self._blocks.append(random.randbytes(self._block_size))

    def do_run(self, run_identifier: str) -> str:
        while not self._state_thread_evt.is_set():
            # Check if rate limited
            if not self.can_send_record():
                time.sleep(0.001)
                continue
            # Send data
            data_record = self.new_data_record()
            for block in self._blocks:
                data_record.add_block(block)
            self.send_data_record(data_record)
        return "Finished run"
