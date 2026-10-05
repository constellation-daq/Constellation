"""
SPDX-FileCopyrightText: 2025 L. Forthomme, DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2

Provides the satellite implementation for the LeCroy/LeCrunch
Constellation interface
"""

import datetime
import struct
from functools import partial
from typing import Any

import LeCrunch3
import numpy as np

from constellation.core.commandmanager import cscp_requestable
from constellation.core.configuration import Configuration
from constellation.core.monitoring import schedule_metric
from constellation.core.protocol.cscp1 import SatelliteState
from constellation.core.transmitter_satellite import TransmitterSatellite

# Name of the per-channel waveform metric, e.g. WAVEFORM_C1
WAVEFORM_METRIC = "WAVEFORM_C{channel}"


class LeCroySatellite(TransmitterSatellite):
    _scope: LeCrunch3.LeCrunch3
    _settings: dict[str, bytes]
    _channels: list[int] = []
    _sequence_mode: bool = False
    _num_sequences: int = 1
    _num_triggers_acquired: int = 0

    _last_waveform: dict[int, list[float]] = {}
    _horiz_interval: float = 0.0
    _horiz_offset: float = 0.0

    def do_initializing(self, configuration: Configuration) -> str:
        self.log.info("Received configuration with parameters: %s", ", ".join(configuration.get_keys()))

        ip_address = configuration.get_str("ip_address")
        port = configuration.get_int("port", 1861, min_val=0, max_val=65535)
        timeout = configuration.get_num("timeout", 5.0)
        num_sequences = configuration.get_int("nsequence", 1)

        try:
            self._scope = LeCrunch3.LeCrunch3(str(ip_address), port=int(port), timeout=float(timeout))
        except ConnectionRefusedError as e:
            raise RuntimeError(f"Connection refused to {ip_address}:{port}") from e

        self._configure_sequences(num_sequences)

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

        self.reset_metrics()
        self._last_waveform = {}
        for channel in self._channels:
            self.register_scheduled_metric(
                WAVEFORM_METRIC.format(channel=channel),
                "V",
                f"Most recent waveform of channel {channel}",
                1,
                partial(self._last_waveform.get, channel),
                [SatelliteState.RUN],
            )

        return f"Connected to scope at {ip_address}"

    def do_reconfigure(self, configuration: Configuration) -> str:
        if "nsequence" in configuration:
            num_sequences = configuration.get_int("nsequence")
            self._scope.clear()
            self._configure_sequences(num_sequences)
        return "Successfully reconfigured scope"

    def do_run(self) -> str:
        num_sequences_acquired = 0
        self._num_triggers_acquired = 0
        self._last_waveform.clear()
        while not self.stop_requested():
            try:
                self._scope.trigger()
                first_channel = True
                event_payload = np.array([])
                for channel in self._channels:
                    wave_desc, trg_times, trg_offsets, wave_array = self._scope.get_waveform_all(channel)
                    num_samples = None
                    if first_channel:
                        event_payload = trg_times
                        num_samples = wave_desc["wave_array_count"] // self._num_sequences
                        event_payload = np.append(event_payload, num_samples)
                        first_channel = False
                        self._horiz_interval = float(wave_desc["horiz_interval"])
                        self._horiz_offset = float(wave_desc["horiz_offset"])
                    wave_array = (
                        wave_array * wave_desc["vertical_gain"] - wave_desc["vertical_offset"]
                    )  # already transform to V
                    event_payload = np.append(event_payload, trg_offsets)
                    event_payload = np.append(event_payload, wave_array)
                    if num_samples:
                        self._last_waveform[channel] = wave_array[-num_samples:].tolist()
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

    def do_landing(self) -> str:
        self._scope.clear()
        self._scope.sock.close()
        self.log.info("Landing...")
        return "Landed"

    @cscp_requestable([SatelliteState.RUN])
    def get_num_triggers(self) -> tuple[str, int, dict[str, Any]]:
        return f"Number of triggers: {self._num_triggers_acquired}", self._num_triggers_acquired, {}

    @cscp_requestable([SatelliteState.INIT, SatelliteState.ORBIT])
    def calibrate(self) -> tuple[str, int, dict[str, Any]]:
        """Run the internal self-calibration of the scope, returns its status code (0 = passed)"""
        self._scope.send("*CAL?")
        reply = self._scope.recv().strip()
        status = int(reply.split(b" ")[-1])
        if status != 0:
            self.log.warning(f"Scope self-calibration returned status {status}")
        return f"Scope self-calibration status {status}", status, {}

    @schedule_metric("", 1, [SatelliteState.RUN])
    def NUM_TRIGGERS(self) -> int | None:
        return self._num_triggers_acquired

    @schedule_metric("s", 1, [SatelliteState.RUN])
    def SAMPLING_PERIOD(self) -> float | None:
        """Time between two samples of the waveform metrics"""
        return self._horiz_interval if self._horiz_interval > 0 else None

    @schedule_metric("s", 1, [SatelliteState.RUN])
    def SAMPLE_OFFSET(self) -> float | None:
        """Time of the first waveform sample relative to the trigger"""
        return self._horiz_offset if self._horiz_interval > 0 else None

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
