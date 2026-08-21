"""
SPDX-FileCopyrightText: 2025 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2
"""

from constellation.core.commandmanager import cscp_requestable
from constellation.core.message.cdtp2 import DataRecord
from constellation.core.receiver_satellite import ReceiverSatellite
from constellation.core.configuration import Configuration
from typing import Any
from pathlib import Path
import numpy as np
import io, time

class EtrocReceiver(ReceiverSatellite):

    # 1. Centralize standard configurations
    DEFAULT_CONFIG = {
        "output_path": "data",
        "translate": 1,
        "skip_fillers": 0,
        "keep_time": 1,
        "flush_interval": 10.0,
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
        "frame_trailer": 0x0,
    }

    FIXED_PATTERN_SIZES = {
        "clk2_filler":   12,
        "fifo_filler":   12,
        "time_filler":   12,
        "event_header":  28,
        "firmware_key":  4,
        "event_trailer": 6,
        "frame_header":  18,
        "frame_trailer": 1,
        "frame_data":    1,
    }

    BUFFER_SHIFTS = {1: 24, 2: 16, 3: 8, 4: 0}

    def do_initializing(self, config: Configuration) -> str:
        """Initialize and configure the satellite."""

        for key, default_value in self.DEFAULT_CONFIG.items():
            # config.get() automatically sets the default if missing
            # and pulls the value from the config file!
            # Adding 'return_type' ensures it stays type-safe!
            value = config.get(key, default_value, return_type=type(default_value))

            # Set the variable on the class
            setattr(self, key, value)

        # Determine file size limit (20 MB for binary, 50,000 lines for text)
        if self.translate:
            self.file_size_limit = 50000       # 50,000 lines for .nem text
        else:
            self.file_size_limit = 20 * 10**6  # 20 MB for raw .bin

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
        self.translate_state[1] = ""
        self.translate_state[2] = -1
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
        self.last_flush = time.monotonic()
        self._reset_params()

        # Determine extension and open the first file
        extension = "nem" if self.translate else "bin"
        self.file_name_pattern = f"{{run_identifier}}/file_{{date}}.{extension}"
        self.outfile = self._open_file()

        self.log.info(f"Started Run {run_identifier}. File opened.")

    def do_stopping(self) -> None:
        """Runs when the Run ends. We safely close the file here."""

        if self.outfile:
            self._close_file(self.outfile)
            self.outfile = None
        self.log.info(f"Stopped Run {self.run_identifier}. File closed safely.")

    @cscp_requestable
    def get_data_rate(self) -> tuple[str, Any, dict[str, Any]]:
        return f"{self.data_rate:.3g} Gbps", self.data_rate, {}

    def receive_bor(self, sender: str, user_tags: dict[str, Any], configuration: dict[str, Any]) -> None:
        pass

    def receive_data(self, sender: str, data_record: DataRecord) -> None:
        """Called automatically by the framework every time a packet arrives."""
        if not self.outfile:
            return  # Safety check: drop data if the file isn't open yet

        # DataRecords can contain multiple blocks. We loop through them to get our bytes.
        for raw_bytes in data_record.blocks:

            # 1. INSTANT DECODING (Zero-Copy)
            # We tell NumPy to lay a view over the raw bytes and treat them
            # as Big-Endian 32-bit unsigned integers ('>u4')
            payload = np.frombuffer(raw_bytes, dtype='>u4')

            # 2. Route to the correct processing logic
            if not self.translate:
                self._write_untranslated(payload)
            else:
                self._translate_and_write(payload)

    def receive_eor(self, sender: str, user_tags: dict[str, Any], run_metadata: dict[str, Any]) -> None:
        pass

    def _rotate_file(self) -> None:
        """Forces a clean file rotation."""
        # 1. Safely close the old file
        self._close_file(self.outfile)

        # 2. Reset counters and start a fresh file
        self.file_size = 0
        self.file_counter += 1
        self.outfile = self._open_file() # Ensure _open_file utilizes self.file_counter
        self.last_flush = time.monotonic()

    def _write_untranslated(self, payload: np.ndarray) -> None:
        """Handles writing raw binary (.bin) files."""

        if self.skip_fillers:
            filtered_payload = []

            for x in payload:
                is_trailer = False

                if (x >> (32 - self.FIXED_PATTERN_SIZES["event_header"])) == self.FIXED_PATTERNS["event_header"]:
                    self.translate_state[0] = True # in event
                elif (x >> (32 - self.FIXED_PATTERN_SIZES["event_trailer"])) == self.FIXED_PATTERNS["event_trailer"]:
                    self.translate_state[0] = False # not in event
                    is_trailer = True # Flag that we hit a clean boundary

                # Filtering logic
                if not self.translate_state[0] and not is_trailer:
                    if self.keep_time:
                        if (x >> 20) not in [self.FIXED_PATTERNS["fifo_filler"], 0x555]:
                            filtered_payload.append(x)
                    else:
                        if (x >> 20) not in [self.FIXED_PATTERNS["fifo_filler"], 0x555, self.FIXED_PATTERNS["time_filler"], self.FIXED_PATTERNS["clk2_filler"]]:
                            filtered_payload.append(x)
                else:
                    filtered_payload.append(x)

                # --- NEW: PENDING FILE ROTATION LOGIC ---
                if is_trailer:
                    # Check if writing this buffer pushes us over the size limit (4 bytes per int)
                    if self.file_size + (len(filtered_payload) * 4) >= self.file_size_limit:
                        # 1. Flush the current buffered event to the old file
                        self.outfile.write(b''.join(int(val).to_bytes(4, 'little') for val in filtered_payload))

                        # 2. Rotate the files (this should also reset self.file_size = 0)
                        self._rotate_file()

                        # 3. Clear the buffer so it doesn't get written twice
                        filtered_payload.clear()
                # ----------------------------------------

            # Write anything left over in the buffer at the end of the chunk
            if filtered_payload:
                self.outfile.write(b''.join(int(x).to_bytes(4, 'little') for x in filtered_payload))
                self.file_size += 4 * len(filtered_payload)

        else:
            # No filtering: Write the NumPy array directly as Little-Endian bytes
            # .astype('<u4') ensures Little-Endian, .tobytes() converts it instantly
            raw_little_endian = payload.astype('<u4').tobytes()
            self.outfile.write(raw_little_endian)
            self.file_size += len(raw_little_endian)

    def _translate_and_write(self, payload: np.ndarray) -> None:
        """Translates raw 32-bit integers into human-readable ETROC2 format (.nem)."""

        # BULLET 2: Initialize the holding buffer for batch writing
        output_buffer = []

        for line_int in payload:
            # Currently outside of an event
            if not self.translate_state[0]:

                # FIFO or fixed TIME Filler
                if (line_int >> (32 - self.FIXED_PATTERN_SIZES["fifo_filler"]) == self.FIXED_PATTERNS["fifo_filler"] or
                    line_int >> (32 - self.FIXED_PATTERN_SIZES["time_filler"]) == self.FIXED_PATTERNS["time_filler"]):

                    filler_type = "FIFO" if (line_int >> (32 - self.FIXED_PATTERN_SIZES["fifo_filler"]) == self.FIXED_PATTERNS["fifo_filler"]) else "CLOCK"

                    # BULLET 1: Pure Bitwise masking (Extremely Fast)
                    # Mask bottom 20 bits: 0xFFFFF
                    payload_20bit = line_int & 0xFFFFF

                    if self.translate_state[2] != payload_20bit:
                        status1 = (payload_20bit >> 16) & 0xF
                        status2 = (payload_20bit >> 8) & 0xFF
                        reset_counter = payload_20bit & 0xFF

                        # self.log.info(f"Link Status: {status1:04b} {status2:08b}, Reset Counter: {reset_counter}")
                        self.translate_state[2] = payload_20bit

                    if not self.skip_fillers:
                        # We still format strings for the output file, but we do it instantly here
                        status1 = (payload_20bit >> 16) & 0xF
                        status2 = (payload_20bit >> 8) & 0xFF
                        reset_counter = payload_20bit & 0xFF
                        output_buffer.append(f"{filler_type} {status1:04b} {status2:08b} {reset_counter}")

                    self.translate_state[1] = "FILLER"

                # CLOCK2 Filler
                elif line_int >> (32 - self.FIXED_PATTERN_SIZES["clk2_filler"]) == self.FIXED_PATTERNS["clk2_filler"]:
                    if not self.skip_fillers:
                        payload_20bit = line_int & 0xFFFFF
                        output_buffer.append(f"CLOCK2 {payload_20bit:020b}")
                    self.translate_state[1] = "FILLER"

                # Event Header, forces transition into event state
                elif line_int >> (32 - self.FIXED_PATTERN_SIZES["event_header"]) == self.FIXED_PATTERNS["event_header"]:
                    self.translate_state[0] = True
                    self.translate_state[1] = "HEADER_1"

                    # BULLET 1: Pure Bitwise logic for stacking active channels
                    active_bits = line_int & 0xF
                    for ch in range(3, -1, -1): # Loops 3, 2, 1, 0 to stack properly for pop()
                        if (active_bits >> ch) & 1:
                            self.active_channels.append(ch)

            # Currently inside of an event
            else:
                # Upon first entry, check if HEADER_2 found, else bail out
                if self.translate_state[1] == "HEADER_1":
                    if line_int >> (32 - self.FIXED_PATTERN_SIZES["firmware_key"]) == self.FIXED_PATTERNS["firmware_key"]:
                        self.translate_state[1] = "HEADER_2"
                        num_words = (line_int >> 2) & 0x3FF
                        self.event_stats[1] = -(40 * int(num_words) // -32) # div ceil -(x//(-y))
                        self.event_stats[2] += 1

                        # BULLET 2: Append to buffer instead of writing
                        output_buffer.append(f"EH {(line_int >> 12) & 0xFFFF} {line_int & 0x3} {num_words} {self.event_stats[1]}")
                    else:
                        self._reset_params()
                        output_buffer.append("BROKEN EVENT HEADER!")

                # Translate ETROC2 Frames after HEADER_2
                elif self.translate_state[1] == "HEADER_2":
                    self.event_stats[2] += 1

                    # Stitching 32-bit words into the 64-bit buffer
                    self.translate_int = (self.translate_int << 32) + np.uint64(line_int)
                    self.event_stats[0] = (self.event_stats[0] + 1) % 5

                    if self.event_stats[0] > 0:
                        to_be_translated = self.translate_int >> self.BUFFER_SHIFTS[self.event_stats[0]]
                        self.translate_int = self.translate_int & ((1 << self.BUFFER_SHIFTS[self.event_stats[0]]) - 1)

                        # HEADER
                        if to_be_translated >> (40 - self.FIXED_PATTERN_SIZES["frame_header"]) == (self.FIXED_PATTERNS["frame_header"] << 2):
                            try:
                                self.active_channel = self.active_channels.pop()
                            except IndexError:
                                self.active_channel = -1
                            output_buffer.append(f"H {self.active_channel} {(to_be_translated >> 14) & 0xFF} {(to_be_translated >> 12) & 0x3} {to_be_translated & 0xFFF}")

                        # DATA
                        elif to_be_translated >> (40 - self.FIXED_PATTERN_SIZES["frame_data"]) == self.FIXED_PATTERNS["frame_data"]:
                            output_buffer.append(f"D {self.active_channel} {(to_be_translated >> 37) & 0x3} {(to_be_translated >> 29) & 0xF} {(to_be_translated >> 33) & 0xF} {(to_be_translated >> 19) & 0x3FF} {(to_be_translated >> 10) & 0x1FF} {to_be_translated & 0x3FF}")

                        # TRAILER
                        elif to_be_translated >> (40 - self.FIXED_PATTERN_SIZES["frame_trailer"]) == self.FIXED_PATTERNS["frame_trailer"]:
                            output_buffer.append(f"T {self.active_channel} {hex((to_be_translated >> 22) & 0x1FFFF)} {(to_be_translated >> 16) & 0x3F} {(to_be_translated >> 8) & 0xFF} {to_be_translated & 0xFF}")

                        else:
                            output_buffer.append("UNKNOWN 40 bit word!")

                    if self.event_stats[2] == self.event_stats[1]:
                        self.translate_state[1] = "ETROC2"

                # Translate Event Trailer after ETROC2 Frames
                elif self.translate_state[1] == "ETROC2":
                    if line_int >> (32 - self.FIXED_PATTERN_SIZES["event_trailer"]) == self.FIXED_PATTERNS["event_trailer"]:
                        self.translate_state[1] = "TRAILER"
                        output_buffer.append(f"ET {(line_int >> 14) & 0xFFF} {(line_int >> 11) & 0x7} {(line_int >> 8) & 0x7} {line_int & 0xFF}")

                        # --- NEW: CLEAN FILE ROTATION LOGIC ---
                        # We safely reached the end of an event. Are we over the size limit?
                        if self.file_size + len(output_buffer) >= self.file_size_limit:
                            # 1. Flush the current event to the OLD file
                            self.outfile.write("\n".join(output_buffer) + "\n")

                            # 2. Rotate the files
                            self._rotate_file()

                            # 3. Clear the buffer so it doesn't get written again at the end of the loop
                            output_buffer.clear()
                        # --------------------------------------

                    else:
                        output_buffer.append("BROKEN EVENT NO EVENT TRAILER FOUND!")
                    self._reset_params()
                else:
                    self._reset_params()
                    output_buffer.append("BROKEN EVENT... How did we get here?...")

        # BULLET 2: Flush the entire buffer to the hard drive in one single I/O operation
        if output_buffer:
            self.outfile.write("\n".join(output_buffer) + "\n")
            self.file_size += len(output_buffer)

    def _open_file(self) -> io.IOBase:
        """Open the data file safely with directory creation."""
        # 1. Format the raw string
        raw_path_str = self.file_name_pattern.format(
            run_identifier=self.run_identifier,
            date=self.file_counter
        )

        # 2. Let Pathlib combine your output path and the new filename intelligently
        filepath = Path(self.output_path) / Path(raw_path_str)
        directory = filepath.parent

        if filepath.is_file():
            self.log.critical(f"File already exists: {filepath}")
            raise RuntimeError(f"File already exists: {filepath}")

        # 3. Make the directory and open the file
        directory.mkdir(parents=True, exist_ok=True)
        mode = "w" if self.translate else "wb"
        return open(filepath, mode)

    def _close_file(self, outfile: io.IOBase) -> None:
        """Safely flush and close the filehandler."""
        if outfile and not outfile.closed:
            outfile.flush()
            outfile.close()
