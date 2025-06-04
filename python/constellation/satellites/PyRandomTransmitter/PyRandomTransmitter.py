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
        self._frame_size = config.setdefault("frame_size", 1024)
        self._number_of_frames = config.setdefault("number_of_frames", 1)

    def do_launching(self) -> None:
        self._frames = []
        for _ in range(self._number_of_frames):
            self._frames.append(random.randbytes(self._frame_size))

    def do_run(self, run_identifier: str) -> str:
        while not self._state_thread_evt.is_set():
            # Check if rate limited
            if self.check_rate_limited():
                time.sleep(0.001)
                continue
            # Send data
            data_block = self.new_data_block()
            for frame in self._frames:
                data_block.add_frame(frame)
            self.send_data_block(data_block)
        return "Finished run"
