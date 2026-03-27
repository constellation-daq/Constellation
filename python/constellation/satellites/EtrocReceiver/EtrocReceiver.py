"""
SPDX-FileCopyrightText: 2025 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2
"""

from constellation.core.commandmanager import cscp_requestable
from constellation.core.message.cdtp2 import DataRecord
from constellation.core.message.cscp1 import CSCP1Message
from constellation.core.satellite import Satellite
from constellation.core.configuration import Configuration
from typing import Any
from datetime import datetime
from pathlib import Path
import numpy as np
import io, time

class EtrocReceiver(Satellite):

    # 1. Centralize standard configurations
    DEFAULT_CONFIG = {
        "output_path": "data",
        "translate": 1,
        "compressed_binary": 1,
        "skip_fillers": 0,
        "keep_time": 1,
        "flush_interval": 10.0,
        "frame_trailers": {0: 0x17f0f, 1: 0x17f0f, 2: 0x17f0f, 3: 0x17f0f}
    }

    # 2. Define static hardware patterns at the class level
    FIXED_PATTERNS = {
        "clk2_filler":   0x553,     # first 12 bits
        "fifo_filler":   0x556,     # first 12 bits
        "time_filler":   0x559,     # first 12 bits
        "event_header":  0xc3a3c3a, # first 28 bits
        "firmware_key":  0x1,       # first 4  bits
        "event_trailer": 0xb,       # first 6  bits
        "frame_header":  0x3c5c,    # first 16 bits + '00'
        "frame_data":    0x1,
    }

    FIXED_PATTERN_SIZES = {
        "clk2_filler":   12,
        "fifo_filler":   12,
        "time_filler":   12,
        "event_header":  28,
        "firmware_key":  4,
        "event_trailer": 6,
        "frame_header":  18,
        "frame_trailer": 18,
        "frame_data":    1,
    }

    BUFFER_SHIFTS = {1: 24, 2: 16, 3: 8, 4: 0}

    def do_initializing(self, config: Configuration) -> str:
        """Initialize and configure the satellite."""

        # Apply configurations dynamically
        for key, default_value in self.DEFAULT_CONFIG.items():
            setattr(self, key, config.set_default(key, default_value))

        # Setup monitoring
        self._configure_monitoring(2.0)

        # Determine file size limit (20 MB for binary, 50,000 lines for text)
        if self.compressed_binary and not self.translate:
            self.file_size_limit = 20 * 10**6
        else:
            self.file_size_limit = 50000

        # Running variables used during run/write loop
        self.file_counter = 0
        self.file_size = 0
        self.active_channels = []
        self.active_channels_extend = self.active_channels.extend
        self.active_channels_pop = self.active_channels.pop
        self.active_channels_clear = self.active_channels.clear

        self.translate_state = [False, "", ""] # [in_event, previous_state, previous_filler]
        self.event_stats     = [-1, -1, -1]    # [40bit_state, num_32bit_words, current_word]

        # Reset dynamic states
        self._reset_params()

        return "Configured ETROC2Receiver"

    def _reset_params(self) -> None:
        """Helper to reset running state variables between events."""
        self.translate_int = np.uint64(0)
        self.active_channels_clear()
        self.active_channel  = -1
        self.translate_state[0] = False
        self.event_stats[0] = -1
        self.event_stats[1] = -1
        self.event_stats[2] = -1

    def do_starting(self, run_identifier: str) -> None:
        """Runs when a new Run starts. We open the file and reset trackers here."""
        self.run_identifier = run_identifier

        # Reset counters and state for the new run
        self.file_counter = 0
        self.file_size = 0
        self.start_time = time.time()
        self.data_rate = 0.0 # Reset saved rate
        self.last_flush = datetime.now()
        self._reset_params()

        # Determine extension and open the first file
        if self.translate:
            extension = "nem"
        elif self.compressed_binary:
            extension = "bin"
        else:
            extension = "dat"

        self.file_name_pattern = f"{{run_identifier}}/file_{{date}}.{extension}"
        self.outfile = self._open_file()

        self.log.info(f"Started Run {run_identifier}. File opened.")

    def do_stopping(self) -> None:
        """Runs when the Run ends. We safely close the file here."""

        run_duration = time.time() - self.start_time
        if self._drc is not None and run_duration > 0:
            gigabyte_received = 1e-9 * self._drc.bytes_received
            self.data_rate = 8 * gigabyte_received / run_duration
            self.log.status(f"Received {gigabyte_received:.2g} GB in {run_duration:.0f}s ({self.data_rate:.3g} Gbps)")

        if self.outfile:
            self._close_file(self.outfile)
            self.outfile = None
        self.log.info(f"Stopped Run {self.run_identifier}. File closed safely.")

    @cscp_requestable
    def get_data_rate(self, request: CSCP1Message) -> tuple[str, Any, dict[str, Any]]:
        return f"{self.data_rate:.3g} Gbps", self.data_rate, {}

    def receive_data(self, sender: str, data_record: DataRecord) -> None:
        """Called automatically by the framework every time a packet arrives."""
        if not self.outfile:
            return  # Safety check: drop data if the file isn't open yet

        # DataRecords can contain multiple blocks. We loop through them to get our bytes.
        for raw_bytes in data_record:

            # 1. INSTANT DECODING (Zero-Copy)
            # We tell NumPy to lay a view over the raw bytes and treat them
            # as Big-Endian 32-bit unsigned integers ('>u4')
            payload = np.frombuffer(raw_bytes, dtype='>u4')

            # 2. Route to the correct processing logic
            if not self.translate:
                self._write_untranslated(payload)
            else:
                self._translate_and_write(payload)

        # 3. Check if we need to rotate the file or flush to disk
        self._manage_file_state()

    def _manage_file_state(self) -> None:
        """Handles file size limits and timed disk flushes."""
        now = datetime.datetime.now()

        # 1. Do we need to make a new file? (prevent very large single files)
        if self.file_size > self.file_size_limit:
            self._close_file(self.outfile)
            self.file_size = 0
            self.file_counter += 1
            self.outfile = self._open_file()
            self.last_flush = now

        # 2. Is it time to flush data to disk?
        elif self.flush_interval > 0 and (now - self.last_flush).total_seconds() > self.flush_interval:
            self.outfile.flush()
            self.last_flush = now

    def _write_untranslated(self, payload: np.ndarray) -> None:
        """Handles writing raw binary (.bin) or formatted text (.dat) files."""

        if self.compressed_binary:
            if self.skip_fillers:
                # --- Original filtering logic ---
                filtered_payload = []
                for x in payload:
                    if (x >> (32 - self.FIXED_PATTERN_SIZES["event_header"])) == self.FIXED_PATTERNS["event_header"]:
                        self.translate_state[0] = True # in event
                    elif (x >> (32 - self.FIXED_PATTERN_SIZES["event_trailer"])) == self.FIXED_PATTERNS["event_trailer"]:
                        self.translate_state[0] = False # not in event

                    if not self.translate_state[0]: # if not in_event
                        if self.keep_time:
                            if (x >> 20) not in [self.FIXED_PATTERNS["fifo_filler"], 0x555]:
                                filtered_payload.append(x)
                        else:
                            if (x >> 20) not in [self.FIXED_PATTERNS["fifo_filler"], 0x555, self.FIXED_PATTERNS["time_filler"], self.FIXED_PATTERNS["clk2_filler"]]:
                                filtered_payload.append(x)
                    else:
                        filtered_payload.append(x)

                # Write the filtered list as Little-Endian bytes
                self.outfile.write(b''.join(int(x).to_bytes(4, 'little') for x in filtered_payload))
                self.file_size += 4 * len(filtered_payload)

            else:
                # No filtering: Write the NumPy array directly as Little-Endian bytes
                # .astype('<u4') ensures Little-Endian, .tobytes() converts it instantly
                raw_little_endian = payload.astype('<u4').tobytes()
                self.outfile.write(raw_little_endian)
                self.file_size += len(raw_little_endian)

        else:
            # Text format (.dat)
            self.outfile.write("\n".join(format(int(x), '032b') for x in payload) + "\n")
            self.file_size += len(payload)

    def _translate_and_write(self, payload: np.ndarray) -> None:
        """Translates raw 32-bit integers into human-readable ETROC2 format (.nem)."""
        for line_int in payload:
            # Currently outside of an event
            if not self.translate_state[0]:
                # FIFO or fixed TIME Filler
                if (line_int >> (32 - self.FIXED_PATTERN_SIZES["fifo_filler"]) == self.FIXED_PATTERNS["fifo_filler"] or
                    line_int >> (32 - self.FIXED_PATTERN_SIZES["time_filler"]) == self.FIXED_PATTERNS["time_filler"]):

                    binary_text = format(int(line_int), '032b')[self.FIXED_PATTERN_SIZES["fifo_filler"]:]
                    filler_type = "FIFO" if (line_int >> (32 - self.FIXED_PATTERN_SIZES["fifo_filler"]) == self.FIXED_PATTERNS["fifo_filler"]) else "CLOCK"

                    if self.translate_state[2] != binary_text:
                        self.log.info(f"Link Status: {binary_text[0:4]} {binary_text[4:12]}, Reset Counter: {int(binary_text[12:],2)}")
                        self.translate_state[2] = binary_text

                    if not self.skip_fillers:
                        self.outfile.write(f"{filler_type} {binary_text[0:4]} {binary_text[4:12]} {int(binary_text[12:],2)}\n")
                        self.file_size += 1
                    self.translate_state[1] = "FILLER"

                # CLOCK2 Filler
                elif line_int >> (32 - self.FIXED_PATTERN_SIZES["clk2_filler"]) == self.FIXED_PATTERNS["clk2_filler"]:
                    binary_text = format(int(line_int), '032b')[self.FIXED_PATTERN_SIZES["clk2_filler"]:]
                    if not self.skip_fillers:
                        self.outfile.write(f"CLOCK2 {binary_text}\n")
                        self.file_size += 1
                    self.translate_state[1] = "FILLER"

                # Event Header, forces transition into event state
                elif line_int >> (32 - self.FIXED_PATTERN_SIZES["event_header"]) == self.FIXED_PATTERNS["event_header"]:
                    self.translate_state[0] = True
                    self.translate_state[1] = "HEADER_1"
                    binary_text = format(int(line_int) & 0xF, '04b')
                    self.active_channels.extend([key for key, val in enumerate(binary_text[::-1]) if val == '1'][::-1])

            # Currently inside of an event
            else:
                # Upon first entry, check if HEADER_2 found, else bail out
                if self.translate_state[1] == "HEADER_1":
                    if line_int >> (32 - self.FIXED_PATTERN_SIZES["firmware_key"]) == self.FIXED_PATTERNS["firmware_key"]:
                        self.translate_state[1] = "HEADER_2"
                        num_words = (line_int >> 2) & 0x3FF
                        self.event_stats[1] = -(40 * int(num_words) // -32) # div ceil -(x//(-y))
                        self.event_stats[2] += 1
                        self.outfile.write(f"EH {(line_int >> 12) & 0xFFFF} {line_int & 0x3} {num_words} {self.event_stats[1]}\n")
                    else:
                        self._reset_params()
                        self.outfile.write("BROKEN EVENT HEADER!\n")
                    self.file_size += 1

                # Translate ETROC2 Frames after HEADER_2
                elif self.translate_state[1] == "HEADER_2":
                    self.event_stats[2] += 1

                    # Stitching 32-bit words into the 64-bit buffer
                    self.translate_int = (self.translate_int << 32) + np.uint64(line_int)
                    self.event_stats[0] = (self.event_stats[0] + 1) % 5

                    if self.event_stats[0] > 0:
                        to_be_translated = self.translate_int >> self.BUFFER_SHIFTS[self.event_stats[0]]
                        self.translate_int = self.translate_int & ((1 << self.BUFFER_SHIFTS[self.event_stats[0]]) - 1)

                        # HEADER "H {channel} {L1Counter} {Type} {BCID}"
                        if to_be_translated >> (40 - self.FIXED_PATTERN_SIZES["frame_header"]) == (self.FIXED_PATTERNS["frame_header"] << 2):
                            try:
                                self.active_channel = self.active_channels.pop()
                            except IndexError:
                                self.active_channel = -1
                            self.outfile.write(f"H {self.active_channel} {(to_be_translated >> 14) & 0xFF} {(to_be_translated >> 12) & 0x3} {to_be_translated & 0xFFF}\n")

                        # DATA "D {channel} {EA} {ROW} {COL} {TOA} {TOT} {CAL}"
                        elif to_be_translated >> (40 - self.FIXED_PATTERN_SIZES["frame_data"]) == self.FIXED_PATTERNS["frame_data"]:
                            self.outfile.write(f"D {self.active_channel} {(to_be_translated >> 37) & 0x3} {(to_be_translated >> 29) & 0xF} {(to_be_translated >> 33) & 0xF} {(to_be_translated >> 19) & 0x3FF} {(to_be_translated >> 10) & 0x1FF} {to_be_translated & 0x3FF}\n")

                        # TRAILER "T {channel} {Status} {Hits} {CRC}"
                        elif to_be_translated >> (40 - self.FIXED_PATTERN_SIZES["frame_trailer"]) == self.frame_trailers.get(self.active_channel, 0):
                            self.outfile.write(f"T {self.active_channel} {(to_be_translated >> 16) & 0x3F} {(to_be_translated >> 8) & 0xFF} {to_be_translated & 0xFF}\n")

                        else:
                            self.outfile.write("UNKNOWN 40 bit word!\n")
                        self.file_size += 1

                    if self.event_stats[2] == self.event_stats[1]:
                        self.translate_state[1] = "ETROC2"

                # Translate Event Trailer after ETROC2 Frames
                elif self.translate_state[1] == "ETROC2":
                    if line_int >> (32 - self.FIXED_PATTERN_SIZES["event_trailer"]) == self.FIXED_PATTERNS["event_trailer"]:
                        self.translate_state[1] = "TRAILER"
                        self.outfile.write(f"ET {(line_int >> 14) & 0xFFF} {(line_int >> 11) & 0x7} {(line_int >> 8) & 0x7} {line_int & 0xFF}\n")
                    else:
                        self.outfile.write("BROKEN EVENT NO EVENT TRAILER FOUND!\n")
                    self._reset_params()
                    self.file_size += 1
                else:
                    self._reset_params()
                    self.outfile.write("BROKEN EVENT... How did we get here?...\n")
                    self.file_size += 1

    def _open_file(self) -> io.IOBase:
        """Open the data file safely with directory creation."""
        filename_list = self.file_name_pattern.format(
                            run_identifier=self.run_identifier,
                            date=self.file_counter,
                        ).split('/')

        filename = Path(filename_list[-1])
        # Join all preceding parts to form the directory path safely
        directory = Path(self.output_path).joinpath(*filename_list[:-1])

        filepath = directory / filename

        if filepath.is_file():
            self.log.critical(f"File already exists: {filepath}")
            raise RuntimeError(f"File already exists: {filepath}")

        self.log.info(f"Creating file {filename} in {directory}...")

        try:
            directory.mkdir(exist_ok=True)
            # Use "w" for translated text, "wb" for raw binary
            mode = "w" if self.translate or not self.compressed_binary else "wb"
            return open(filepath, mode)
        except Exception as e:
            self.log.critical(f"Unable to create/open file {filepath}: {e}")
            raise RuntimeError(f"Unable to open {filepath}: {e}") from e

    def _close_file(self, outfile: io.IOBase) -> None:
        """Safely flush and close the filehandler."""
        if outfile and not outfile.closed:
            outfile.flush()
            outfile.close()