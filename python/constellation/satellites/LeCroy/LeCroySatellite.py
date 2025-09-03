"""
SPDX-FileCopyrightText: 2025 L. Forthomme, DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2

Provides the satellite implementation for the LeCroy/LeCrunch
Constellation interface
"""

import datetime
import struct
from typing import Any

import LeCrunch3
import numpy as np

from constellation.core.cmdp import MetricsType
from constellation.core.commandmanager import cscp_requestable
from constellation.core.configuration import Configuration
from constellation.core.message.cscp1 import CSCP1Message, SatelliteState
from constellation.core.monitoring import schedule_metric
from constellation.core.transmitter_satellite import TransmitterSatellite


class LeCroySatellite(TransmitterSatellite):
    _scope = None
    _settings = None
    _channels = None
    _sequence_mode: bool = False
    _num_sequences: int = 1
    _num_triggers_acquired: int = 0

    def do_initializing(self, configuration: Configuration) -> str:
        self.log.info("Received configuration with parameters: %s", ", ".join(configuration.get_keys()))

        ip_address = configuration["ip_address"]
        port = configuration.setdefault("port", 1861)
        timeout = configuration.setdefault("timeout", 5.0)

        try:
            self._scope = LeCrunch3.LeCrunch3(str(ip_address), port=int(port), timeout=float(timeout))
        except ConnectionRefusedError as e:
            raise RuntimeError(f"Connection refused to {ip_address}:{port} -> {str(e)}")

        self._configure_sequences(configuration.setdefault("nsequence", 1))

        # channels trigger levels and offsets are not expected to change on reconfiguration
        self._channels = self._scope.get_channels()
        channel_offsets = {}
        channel_trigger_levels = {}
        for key, value in self._settings.items():
            if ":OFFSET" in key:
                channel_offsets[key.split(":")[0].replace("C", "")] = float(value.split(b" ")[1])
            elif ":TRIG_LEVEL" in key:
                channel_trigger_levels[key.split(":")[0].replace("C", "")] = float(value.split(b" ")[1])

        self.bor["trigger_delay"] = float(self._settings["TRIG_DELAY"].split(b" ")[1])
        self.bor["sampling_period"] = float(self._settings["TIME_DIV"].split(b" ")[1])
        self.bor["channels"] = ",".join([str(c) for c in self._channels])

        return f"Connected to scope at {ip_address}"

    def do_reconfigure(self, configuration: Configuration) -> str:
        if not self._scope:
            return "Failed to reconfigure. Scope is not connected."
        self._scope.clear()
        self._configure_sequences(configuration.setdefault("nsequence", 1))
        return "Successfully reconfigured scope"

    def do_run(self, payload: Any) -> str:
        num_sequences_acquired = 0
        self._num_triggers_acquired = 0
        while not self._state_thread_evt.is_set():
            try:
                self._scope.trigger()
                first_channel = True
                event_payload = np.array([])
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
                data_record = self.new_data_record({"dtype": f"{event_payload.dtype}"})
                data_record.add_block(event_payload.tobytes())
                self.send_data_record(data_record)
            except TimeoutError:
                self.log.warning("Timeout encountered while retrieving the sequence.")
                continue
            except (OSError, struct.error) as e:
                self.log.error(str(e))
                self._scope.clear()
                continue
            self._num_triggers_acquired += self._num_sequences
            num_sequences_acquired += 1
            self.log.info(f"Fetched event {self._num_triggers_acquired}/sequence {num_sequences_acquired}")

        self.eor["current_time"] = datetime.datetime.now().timestamp()
        return "Finished acquisition"

    def do_stopping(self) -> str:
        self._scope.clear()
        self.log.info(f"Stopping the run after {self._num_triggers_acquired} event(s)")
        return "Stopped acquisition"

    @cscp_requestable
    def get_num_triggers(self, request: CSCP1Message) -> [str, int, dict[str, Any]]:
        if self.fsm.current_state_value == SatelliteState.RUN:
            return f"Number of triggers: {self._num_triggers_acquired}", self._num_triggers_acquired, {}
        return "Not running", -1, {}

    @schedule_metric("", MetricsType.LAST_VALUE, 10)
    def NUM_TRIGGERS(self) -> int | None:
        if self.fsm.current_state_value == SatelliteState.RUN:
            return self._num_triggers_acquired
        return None

    def _configure_sequences(self, num_sequences: int):
        self._num_sequences = num_sequences
        self._sequence_mode = self._num_sequences > 0
        if self._sequence_mode:
            self._scope.set_sequence_mode(self._num_sequences)
        self._settings = self._scope.get_settings()
        self.log.debug(f"Scope settings: {self._settings}")
        if b"ON" in self._settings["SEQUENCE"]:  # waveforms sequencing enabled
            sequence_count = int(self._settings["SEQUENCE"].split(b",")[1])
            self.log.info(f"Configured scope with sequence count = {sequence_count}")
            if self._num_sequences != sequence_count:  # sanity check
                raise RuntimeError(
                    "Could not configure sequence mode properly: "
                    + f"num_sequences={self._num_sequences} != sequences_count={sequence_count}"
                )
        if self._num_sequences != 1:
            self.log.info(f"Using sequence mode with {self._num_sequences} traces per acquisition")

        # update the beginning-of-run event
        self.bor["num_sequences"] = self._num_sequences
