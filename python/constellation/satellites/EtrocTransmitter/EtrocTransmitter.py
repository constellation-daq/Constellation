"""
SPDX-FileCopyrightText: 2025 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2
"""

from constellation.core.commandmanager import cscp_requestable
from constellation.core.cscp import CSCP1Message
from constellation.core.configuration import Configuration
from constellation.core.transmitter_satellite import TransmitterSatellite
from typing import Any
from . import cmd_interpret
import time, socket

class EtrocTransmitter(TransmitterSatellite):

    DEFAULT_CONFIG = {
        "hostname": "192.168.2.3",
        "port": 1024,
        "firmware": "0001",
        "polarity": 0x4023,
        "timestamp": 0x0000,
        "active_channel": 0x0001,
        "prescale_factor": 2048,
        "counter_duration": 0x0000,
        "triggerbit_delay": 0x1800,
        "fc_delays": 0x0000,
        "data_delays_01": 0x0000,
        "data_delays_23": 0x0000,
        "num_fifo_read": 65536,
        "clear_fifo": 1,
        "reset_counter": 1,
        "fast_command_memo": "Start Triggerbit"
    }

    def _send_fc_sequence(self, reg12_val: int, reg10_val: int, reg9_val: int) -> None:
        """Helper to send a standard Fast Command hardware sequence."""
        cmd_interpret.write_config_reg_decoded(self.connection_socket, "register_12", reg12_val)
        cmd_interpret.write_config_reg_decoded(self.connection_socket, "register_10", reg10_val, self.prescale_factor)
        cmd_interpret.write_config_reg_decoded(self.connection_socket, "register_9", reg9_val)
        cmd_interpret.write_pulse_reg_decoded(self.connection_socket, "fc_init")
        time.sleep(0.01)

    def _execute_fc_command(self, base_reg12: int, loops: int, send_qinj: bool, send_l1a: bool) -> None:
        """Helper to handle QInj and L1A commands using non-overlapping bunches.

        If loops = 1, it executes only the first combination.
        If loops > 1, it loops up to a maximum of 378 times.
        """
        qinj_reg = base_reg12 + 0x5
        l1a_reg = base_reg12 + 0x6

        # Cap the iterations to our safe maximum of 378, ensuring at least 1 run
        num_iterations = min(max(loops, 1), 378)

        for i in range(num_iterations):
            # Determine the bunch (0, 1, or 2) and the offset index within that bunch
            bunch_index = i % 3
            step_index = i // 3

            # 1. Map step_index to the correct bunch values based on bunch_index
            if bunch_index == 0:  # Bunch 1
                qinj_val = 0x005 + (step_index * 4)
                l1a_val = 0x1fd + (step_index * 4)
            elif bunch_index == 1:  # Bunch 2
                qinj_val = 0x3f5 + (step_index * 4)
                l1a_val = 0x5ed + (step_index * 4)
            else:  # bunch_index == 2 (Bunch 3)
                qinj_val = 0x7e5 + (step_index * 4)
                l1a_val = 0x9dd + (step_index * 4)

            # 2. Conditionally send the commands (Option A: QInj first)
            if send_qinj:
                self._send_fc_sequence(qinj_reg, qinj_val, qinj_val)

            if send_l1a:
                self._send_fc_sequence(l1a_reg, l1a_val, l1a_val)

    def configure_memo_FC(self, memo=None) -> None:
        memo_str = memo if memo is not None else self.fast_command_memo
        words = memo_str.split()

        # 1. Parse all flags into a clean dictionary
        flags = {
            "QInj": "QInj" in words,
            "multiple": any("multiple" in w for w in words),
            "L1A": "L1A" in words,
            "BCR": "BCR" in words,
            "Triggerbit": "Triggerbit" in words,
            "Start": "Start" in words,
        }

        # 2. Extract loop count safely
        qinj_loop = 1  # Default to 1 (single run)

        if flags["multiple"]:
            for w in words:
                if "multiple=" in w:
                    try:
                        # Split at "=" and convert the second part to an integer
                        qinj_loop = int(w.split("=")[1])
                    except (ValueError, IndexError):
                        # Fallback to 1 if conversion fails or structure is unexpected
                        qinj_loop = 1

        # 3. Determine base register 12 value
        base_reg12 = 0x0070 if flags["Triggerbit"] else 0x0030

        # --- Hardware Execution Sequence ---

        if flags["Start"]:
            cmd_interpret.write_config_reg_decoded(self.connection_socket, "register_11", 0x0deb)
            time.sleep(0.01)

        # IDLE (Always executed based on original code flow)
        self._send_fc_sequence(base_reg12, 0x000, 0x0deb)

        if flags["BCR"]:
            self._send_fc_sequence(base_reg12 + 0x2, 0x000, 0x000)

        if flags["QInj"] and flags["L1A"]:
            self._execute_fc_command(base_reg12, qinj_loop, send_qinj=True, send_l1a=True)

        # 2. Only QInj active: Self-trigger scan (No L1A)
        elif flags["QInj"]:
            self._execute_fc_command(base_reg12, qinj_loop, send_qinj=True, send_l1a=False)

        # Final signal start
        cmd_interpret.write_pulse_reg_decoded(self.connection_socket, "fc_signal_start")
        time.sleep(0.01)

    def do_initializing(self, config: Configuration) -> str:
        """Configure the Satellite and any associated hardware."""

        for key, default_value in self.DEFAULT_CONFIG.items():
            # config.get() automatically sets the default if missing
            # and pulls the value from the config file!
            # Adding 'return_type' ensures it stays type-safe!
            value = config.get(key, default_value, return_type=type(default_value))

            # Set the variable on the class
            setattr(self, key, value)

        self.connection_socket = None

        if self.prescale_factor not in [2048, 4096, 8192, 16384]:
            raise ValueError(f"Prescale factor must be one of [2048, 4096, 8192, 16384], {self.prescale_factor} not supported")

        self.log.info("Configuration loaded and Defaults set")
        return "Initialized - Configuration loaded and Defaults set"

    def do_launching(self) -> str:
        # 1. Improved error reporting for socket creation/connection
        try:
            self.connection_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        except socket.error as e:
            raise RuntimeError(f"Failed to create socket for EtrocTransmitter Satellite: {e}")

        try:
            self.connection_socket.connect((self.hostname, self.port))
        except socket.error as e:
            raise ConnectionError(f"Failed to connect to IP {self.hostname}:{self.port} - {e}")

        # 2. Cleanly map and write standard registers
        initial_registers = {
            "active_channel": self.active_channel,
            "timestamp": self.timestamp,
            "triggerbit_delay": self.triggerbit_delay,
            "polarity": self.polarity,
            "counter_duration": self.counter_duration,
            "fc_delays": self.fc_delays,
            "data_delays_01": self.data_delays_01,
            "data_delays_23": self.data_delays_23
        }

        for reg_name, value in initial_registers.items():
            cmd_interpret.write_config_reg_decoded(self.connection_socket, reg_name, value)

        # Write special-case register
        cmd_interpret.write_config_reg_decoded(self.connection_socket, "register_10", 0x000, self.prescale_factor)

        if self.clear_fifo:
            self.log.info("Clearing FIFO...")
            cmd_interpret.write_pulse_reg_decoded(self.connection_socket, "clear_fifo")
            time.sleep(2.1)

        self.configure_memo_FC()
        self.log.info("Socket connected, FPGA Registers and Fast Command configured")
        return "Launched - Socket connected, FPGA Registers and Fast Command configured"

    def do_landing(self) -> str:
        self.configure_memo_FC(memo="Triggerbit")

        # 3. Safely shutdown and close the socket without crashing if already disconnected
        if self.connection_socket:
            try:
                self.connection_socket.shutdown(socket.SHUT_RDWR)
            except OSError:
                pass # Ignore if the socket is already dead or closed
            self.connection_socket.close()

        self.log.info("Socket shutdown and closed, Fast Command idling")
        return "Landed - Socket shutdown and closed, Fast Command idling"

    def do_starting(self, run_identifier: str) -> str:
        """
        move to data taking position
        """
        # Packaging the BOR Message
        self.BOR = {
            "start_time": time.strftime("%Y-%m-%d-%H%M%S", time.localtime()),
            "run_identifier": run_identifier,
        }
        time.sleep(0.1)  # add sleep to make sure that everything has stopped

        # FPGA Presteps for DAQ
        if self.reset_counter:
            cmd_interpret.write_pulse_reg_decoded(self.connection_socket, "reset_counter")
            time.sleep(0.1)
            self.log.info("Cleared Event Counter")

        # Start DAQ Session on FPGA
        self.log.info("Starting DAQ Session on FPGA...")

        # Bitwise logic: Clear the bottom 2 bits (~0b11) and set them to '10' (0b10)
        self.timestamp = (self.timestamp & ~0b11) | 0b10
        cmd_interpret.write_config_reg_decoded(self.connection_socket, "timestamp", self.timestamp)
        time.sleep(0.1)

        self.log.debug(f"Status of DAQ Toggle before Start Pulse: {format(cmd_interpret.read_status_reg(self.connection_socket, 5), '016b')}")
        cmd_interpret.write_pulse_reg_decoded(self.connection_socket, "start_DAQ")
        time.sleep(0.1)
        self.log.debug(f"Status of DAQ Toggle after Start Pulse: {format(cmd_interpret.read_status_reg(self.connection_socket, 5), '016b')}")

        return f"Run {run_identifier} Session Started"

    def do_stopping(self) -> str:
        """End the run. Add run metadata for end-of-run event"""
        time.sleep(0.1)
        # Stop DAQ Session on FPGA
        self.log.info("Stopping DAQ on FPGA...")

        # Bitwise logic: Clear the bottom 2 bits (~0b11) and set them to '10' (0b10)
        self.timestamp = (self.timestamp & ~0b11) | 0b10
        cmd_interpret.write_config_reg_decoded(self.connection_socket, "timestamp", self.timestamp)
        time.sleep(0.1)

        self.log.debug(f"Status of DAQ Toggle before Stop Pulse: {format(cmd_interpret.read_status_reg(self.connection_socket, 5), '016b')}")
        cmd_interpret.write_pulse_reg_decoded(self.connection_socket, "stop_DAQ")
        time.sleep(0.1)
        self.log.debug(f"Status of DAQ Toggle after Stop Pulse: {format(cmd_interpret.read_status_reg(self.connection_socket, 5), '016b')}")

        # Packaging the EOR Message
        self.EOR = {
            "stop_time": time.strftime("%Y-%m-%d-%H%M%S", time.localtime()),
            "register_7": cmd_interpret.read_config_reg(self.connection_socket, 7),
            "register_16": cmd_interpret.read_config_reg(self.connection_socket, 16),
        }
        return f"Run {self.run_identifier} Stopped, EOR Sent"

    def do_run(self) -> str:
        """Run the satellite. Collect data from buffers and send it."""
        # 1. Use the framework's built-in stop request flag
        while not self.stop_requested():

            # 2. Check if rate limited
            if not self.can_send_record():
                time.sleep(0.001)
                continue

            # 3. Your Hardware Logic: Read the main DAQ-loop
            raw_bytes = cmd_interpret.read_data_fifo(self.connection_socket, self.num_fifo_read)

            # 3. Handle empty buffer (exactly as you had it before)
            if not raw_bytes:
                self.log.debug("No data in buffer! Will try to read again")
                time.sleep(0.1)
                continue

            data_record = self.new_data_record()
            data_record.add_block(raw_bytes)
            self.send_data_record(data_record)

        return "Finished acquisition"

    @cscp_requestable
    def get_config_register(self, request: CSCP1Message) -> tuple[str, Any, dict]:
        reg = request.payload
        return "FPGA is Ready", format(cmd_interpret.read_config_reg(self.connection_socket, reg), '016b'), {}

    @cscp_requestable
    def get_status_register(self, request: CSCP1Message) -> tuple[str, Any, dict]:
        reg = request.payload
        return "FPGA is Ready", format(cmd_interpret.read_status_reg(self.connection_socket, reg), '016b'), {}

    @cscp_requestable
    def set_data_phase_delay(self, request: CSCP1Message) -> tuple[str, Any, dict]:
        data_delay = max(0, min(request.payload, 63)) # 6 bits max

        # Clear bits 7-12 (6 bits), then set them to data_delay
        mask = ~(0x3F << 7)
        self.timestamp = (self.timestamp & mask) | (data_delay << 7)

        cmd_interpret.write_config_reg_decoded(self.connection_socket, "timestamp", self.timestamp)
        return "FPGA Reg 13 Set, Data Delay Set", f"0x{cmd_interpret.read_config_reg(self.connection_socket, 13):04x}", {}

    @cscp_requestable
    def set_data_phase_channel_delay(self, request: CSCP1Message) -> tuple[str, Any, dict]:
        data_delay = max(0, min(request.payload[0], 39)) # Original code clamped to 39
        channel = max(0, min(request.payload[1], 3))

        shift = 0 if channel in (0, 2) else 8
        mask = ~(0x3F << shift) # 6-bit mask

        if channel < 2:
            self.data_delays_01 = (self.data_delays_01 & mask) | (data_delay << shift)
            cmd_interpret.write_config_reg_decoded(self.connection_socket, "data_delays_01", self.data_delays_01)
            return "FPGA Reg 5 Set, Data Delay Set", f"0x{cmd_interpret.read_config_reg(self.connection_socket, 5):04x}", {}
        else:
            self.data_delays_23 = (self.data_delays_23 & mask) | (data_delay << shift)
            cmd_interpret.write_config_reg_decoded(self.connection_socket, "data_delays_23", self.data_delays_23)
            return "FPGA Reg 6 Set, Data Delay Set", f"0x{cmd_interpret.read_config_reg(self.connection_socket, 6):04x}", {}

    @cscp_requestable
    def set_fc_phase_delay(self, request: CSCP1Message) -> tuple[str, Any, dict]:
        fc_delay = max(0, min(request.payload, 63))

        # Clear bits 10-15 (6 bits) and set to fc_delay
        mask = ~(0x3F << 10)
        self.counter_duration = (self.counter_duration & mask) | (fc_delay << 10)

        cmd_interpret.write_config_reg_decoded(self.connection_socket, "counter_duration", self.counter_duration)
        return "FPGA Reg 7 Set, FC Phase Delay Set", f"0x{cmd_interpret.read_config_reg(self.connection_socket, 7):04x}", {}

    @cscp_requestable
    def set_fc_phase_channel_delay(self, request: CSCP1Message) -> tuple[str, Any, dict]:
        fc_delay = max(0, min(request.payload[0], 31))
        channel = max(0, min(request.payload[1], 3))

        # 1. Set the 4-bit delay in fc_delays (Reg 4)
        shift_fc = 4 * channel
        mask_fc = ~(0xF << shift_fc)
        self.fc_delays = (self.fc_delays & mask_fc) | ((fc_delay & 0xF) << shift_fc)
        cmd_interpret.write_config_reg_decoded(self.connection_socket, "fc_delays", self.fc_delays)

        # 2. Set the MSB (5th bit) in data_delays_01 (Reg 5)
        msb = (fc_delay >> 4) & 0x1
        shift_msb = {0: 6, 1: 7, 2: 14, 3: 15}.get(channel, -1)

        if shift_msb != -1:
            mask_msb = ~(1 << shift_msb)
            self.data_delays_01 = (self.data_delays_01 & mask_msb) | (msb << shift_msb)
            cmd_interpret.write_config_reg_decoded(self.connection_socket, "data_delays_01", self.data_delays_01)

        return "FPGA Reg 4 and 5 Set, FC Phase Delay Set", [f"0x{cmd_interpret.read_config_reg(self.connection_socket, 4):04x}", f"0x{cmd_interpret.read_config_reg(self.connection_socket, 5):04x}"], {}

    @cscp_requestable
    def set_fc_bit_delay(self, request: CSCP1Message) -> tuple[str, Any, dict]:
        bit_delay = max(0, min(request.payload, 15)) # Assuming 4 bits max based on original logic

        # Clear bits 10-13 (4 bits) and set to bit_delay
        mask = ~(0xF << 10)
        self.polarity = (self.polarity & mask) | (bit_delay << 10)

        cmd_interpret.write_config_reg_decoded(self.connection_socket, "polarity", self.polarity)
        return "FPGA Reg 14 Set, FC Bit Delay Set", f"0x{cmd_interpret.read_config_reg(self.connection_socket, 14):04x}", {}

    @cscp_requestable
    def set_ledpage(self, request: CSCP1Message) -> tuple[str, Any, dict]:
        ledpage = max(0, min(request.payload, 5))

        # Clear bits 2-4 (3 bits) and set to ledpage
        mask = ~(0x7 << 2)
        self.timestamp = (self.timestamp & mask) | (ledpage << 2)

        cmd_interpret.write_config_reg_decoded(self.connection_socket, "timestamp", self.timestamp)
        return "FPGA Reg 13 Set, Led Page Set", format(cmd_interpret.read_config_reg(self.connection_socket, 13), '016b'), {}

    @cscp_requestable
    def set_active_channel(self, request: CSCP1Message) -> tuple[str, Any, dict]:
        self.active_channel = request.payload
        cmd_interpret.write_config_reg_decoded(self.connection_socket, "active_channel", self.active_channel)
        return "FPGA Reg 15 Set, active_channel Set", format(cmd_interpret.read_config_reg(self.connection_socket, 15), '016b'), {}

    @cscp_requestable
    def set_fast_command_memo(self, request: CSCP1Message) -> tuple[str, Any, dict]:
        self.fast_command_memo = request.payload
        self.configure_memo_FC()
        return "Fast Command Configured", self.fast_command_memo, {}

    # ==========================================
    # CSCP Command Permission Checks
    # ==========================================

    def _is_orbit_state(self, request: CSCP1Message) -> bool:
        """Helper to ensure commands are only allowed in the ORBIT state."""
        return self.fsm.current_state.id in ["ORBIT"]

    # Map the required framework permission checks to our single helper
    _get_config_register_is_allowed = _is_orbit_state
    _get_status_register_is_allowed = _is_orbit_state
    _set_data_phase_delay_is_allowed = _is_orbit_state
    _set_data_phase_channel_delay_is_allowed = _is_orbit_state
    _set_fc_phase_delay_is_allowed = _is_orbit_state
    _set_fc_phase_channel_delay_is_allowed = _is_orbit_state
    _set_fc_bit_delay_is_allowed = _is_orbit_state
    _set_ledpage_is_allowed = _is_orbit_state         # Fixed the naming bug here!
    _set_active_channel_is_allowed = _is_orbit_state
    _set_fast_command_memo_is_allowed = _is_orbit_state