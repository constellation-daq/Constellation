"""
SPDX-FileCopyrightText: 2025 L. Forthomme, DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2

Provides the satellite implementation for the LeCroy/LeCrunch
Constellation interface
"""

import socket
import struct
from typing import Any

import LeCrunch3
import numpy as np

from constellation.core.configuration import Configuration
from constellation.core.datasender import DataSender


class LeCroySatellite(DataSender):
    _scope = None
    _settings = None
    _channels = None
    _num_sequences = 1
    _sequence_mode = False

    def do_initializing(self, configuration: Configuration) -> str:
        self.log.info("Received configuration with parameters: %s", ", ".join(configuration.get_keys()))

        ip_address = configuration["ip_address"]
        port = configuration.setdefault("port", 1861)
        timeout = configuration.setdefault("timeout", 5.0)
        self._num_sequences = configuration.setdefault("nsequence", 1)

        try:
            self._scope = LeCrunch3.LeCrunch3(str(ip_address), port=int(port), timeout=float(timeout))
            self._scope.clear()
        except ConnectionRefusedError as e:
            self.log.error(f"Connection refused to {ip_address}:{port} -> {str(e)}")
            return ""

        if self._num_sequences > 0:
            self._scope.set_sequence_mode(self._num_sequences)
            self._sequence_mode = True

        self._channels = self._scope.get_channels()
        self._settings = self._scope.get_settings()
        channel_offsets = {}
        channel_trigger_levels = {}
        for key, value in self._settings.items():
            if ":OFFSET" in key:
                channel_offsets[key.split(":")[0].replace("C", "")] = float(value.split(b" ")[1])
            elif ":TRIG_LEVEL" in key:
                channel_trigger_levels[key.split(":")[0].replace("C", "")] = float(value.split(b" ")[1])

        if b"ON" in self._settings["SEQUENCE"]:  # waveforms sequencing enabled
            sequence_count = int(self._settings["SEQUENCE"].split(b",")[1])
            self.log.info(f"Configured scope with sequence count = {sequence_count}")
            if self._num_sequences != sequence_count:  # sanity check
                self.log.error(
                    "Could not configure sequence mode properly: "
                    + f"num_sequences={self._num_sequences} != sequences_count={sequence_count}"
                )
        if self._num_sequences != 1:
            self.log.info(f"Using sequence mode with {self._num_sequences} traces per acquisition")

        self.BOR["trigger_delay"] = float(self._settings["TRIG_DELAY"].split(b" ")[1])
        self.BOR["sampling_period"] = float(self._settings["TIME_DIV"].split(b" ")[1])
        self.BOR["channels"] = ",".join([str(c) for c in self._channels])
        self.BOR["num_sequences"] = self._num_sequences

        self.log.debug("Scope settings: {}".format(self._settings))

        return f"Connected to scope at {ip_address}"

    def do_run(self, payload: Any) -> str:
        num_sequences_acquired = 0
        num_events_acquired = 0
        while not self._state_thread_evt.is_set():
            try:
                self._scope.trigger()
                first_channel = True
                for channel in self._channels:
                    wave_desc, trg_times, trg_offsets, wave_array = self._scope.get_waveform_all(channel)
                    if first_channel:
                        event_payload = trg_times
                        num_samples = wave_desc["wave_array_count"] // self._num_sequences
                        event_payload = np.append(event_payload, num_samples)
                        first_channel = False
                    wave_array = (
                        wave_array * wave_desc["vertical_gain"] - wave_desc["vertical_offset"]
                    )  # already transform to V
                    event_payload = np.append(event_payload, trg_offsets)
                    event_payload = np.append(event_payload, wave_array)
                self.data_queue.put((event_payload.tobytes(), {"dtype": f"{event_payload.dtype}"}))
            except (socket.error, struct.error) as e:
                self.log.error(str(e))
                self._scope.clear()
                continue
            num_events_acquired += self._num_sequences
            num_sequences_acquired += 1
            self.log.info(f"Fetched event {num_events_acquired}/sequence {num_sequences_acquired}")

        return "Finished acquisition"
