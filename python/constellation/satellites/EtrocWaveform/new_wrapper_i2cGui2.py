import pandas as pd

import logging
import time, sys

from datetime import datetime
from tqdm import tqdm

# ANSI colors
GREEN = '\033[32m'
RED   = "\033[31m"
RESET = '\033[0m'

# =======================================================================
# WORKAROUND: Prevent i2c_gui2 from crashing on Constellation's TRACE level
# =======================================================================
_saved_attrs = {}
for attr in ['TRACE', 'trace']:
    if hasattr(logging, attr):
        _saved_attrs[attr] = getattr(logging, attr)
        delattr(logging, attr)

_logger_cls = logging.getLoggerClass()
if hasattr(_logger_cls, 'trace'):
    _saved_attrs['cls_trace'] = getattr(_logger_cls, 'trace')
    delattr(_logger_cls, 'trace')

import i2c_gui2

# Restore Constellation's original TRACE settings for the framework
if 'TRACE' in _saved_attrs: logging.TRACE = _saved_attrs['TRACE']
if 'trace' in _saved_attrs: logging.trace = _saved_attrs['trace']
if 'cls_trace' in _saved_attrs: setattr(_logger_cls, 'trace', _saved_attrs['cls_trace'])
# =======================================================================

class i2c_connection:
    def __init__(self, port: str, chip_addresses: list, ws_addresses: list, chip_names: list, clock: int = 100):
        self.chip_addresses = chip_addresses
        self.ws_addresses = ws_addresses
        self.chip_names = chip_names

        # 1. Instance-Specific Caches
        # Keeps 'chips' bound to this specific object, not shared globally across the class
        self._chips = {}
        self.BL_df = {addr: pd.DataFrame() for addr in chip_addresses}

        # 2. Simplified Delay Tracking
        self.fc_delays = {addr: {'clk': 1, 'data': 1} for addr in chip_addresses}

        # 3. Build YOUR specific logger manually
        self.logger = logging.getLogger("I2C_Manager")
        self.logger.setLevel(logging.INFO)
        self.logger.propagate = False  # Prevents it from touching the global root

        if not self.logger.handlers:
            handler = logging.StreamHandler(sys.stdout)
            formatter = logging.Formatter('%(asctime)s - %(levelname)s - %(message)s', datefmt='%Y-%m-%d %H:%M:%S')
            handler.setFormatter(formatter)
            self.logger.addHandler(handler)

        # 4. Build the dummy logger for the chip object
        self.i2c_logger = logging.getLogger("Silent_Chip")
        self.i2c_logger.setLevel(logging.CRITICAL)
        self.i2c_logger.propagate = False
        if not self.i2c_logger.handlers:
            self.i2c_logger.addHandler(logging.NullHandler())

        self.conn = i2c_gui2.USB_ISS_Helper(port, clock, dummy_connect=False)

    # ==========================================
    # THE GATEKEEPER
    # ==========================================
    def _resolve_chip(self, addr: int) -> i2c_gui2.ETROC2_Chip:
        if addr not in self._chips:
            try:
                idx = self.chip_addresses.index(addr)
                ws_addr = self.ws_addresses[idx]
            except ValueError:
                ws_addr = None

            # Use self.logger here instead of self.chip_logger
            self._chips[addr] = i2c_gui2.ETROC2_Chip(addr, ws_addr, self.conn, self.i2c_logger)

        return self._chips[addr]

    def config_TDC_window_ranges_in_memory(self, addr: int, window_dict: dict = None):
        chip = self._resolve_chip(addr)

        window = window_dict or {
            "upperTOATrig": 0x3ff, "lowerTOATrig": 0x000,
            "upperTOTTrig": 0x1ff, "lowerTOTTrig": 0x000,
            "upperCalTrig": 0x3ff, "lowerCalTrig": 0x000,
            "upperTOA": 0x3ff,     "lowerTOA": 0x000,
            "upperTOT": 0x1ff,     "lowerTOT": 0x000,
            "upperCal": 0x3ff,     "lowerCal": 0x000,
        }
        for key, value in window.items():
            chip.set_decoded_value("ETROC2", "Pixel Config", key, value)

    # ==========================================
    # LEVEL 1: SINGLE-CHIP HARDWARE METHODS
    # ==========================================
    def disable_all_pixels(self, addr: int, power_mode: str = 'high'):
        """
        Disables all pixels for exactly ONE chip.
        No loops, no external logic. Just direct hardware control.
        """
        chip = self._resolve_chip(addr)
        chip.row = 0
        chip.col = 0

        # Fast dictionary lookup replaces if/elif chains
        power_map = {'high': 0b000, '010': 0b010, '101': 0b101, 'low': 0b111}
        ibsel = power_map.get(power_mode, 0b000)

        chip.read_all_block("ETROC2", "Pixel Config")

        pixel_config = {
            "disDataReadout": 1, "QInjEn": 0, "disTrigPath": 1,
            "upperTOATrig": 0x000, "lowerTOATrig": 0x000,
            "upperTOTTrig": 0x1ff, "lowerTOTTrig": 0x1ff,
            "upperCalTrig": 0x3ff, "lowerCalTrig": 0x3ff,
            "upperTOA": 0x000, "lowerTOA": 0x000,
            "upperTOT": 0x1ff, "lowerTOT": 0x1ff,
            "upperCal": 0x3ff, "lowerCal": 0x3ff,
            "enable_TDC": 0,
            "IBSel": ibsel,
            "Bypass_THCal": 1,
            "TH_offset": 0x3f,
            "DAC": 0x3ff,
        }

        for key, value in pixel_config.items():
            chip.set_decoded_value("ETROC2", "Pixel Config", key, value)

        chip.broadcast = True
        chip.write_all_block("ETROC2", "Pixel Config")
        chip.broadcast = False

        self.logger.info(f'  SUCCESS - Disable all 256 pixels of ({hex(addr)})')

    # ==========================================
    # LEVEL 2: BATCH OPERATIONS / ORCHESTRATORS
    # ==========================================
    def batch_disable_all_chips(self, power_mode: str = 'high'):
        """
        A thin wrapper that loops over the single-chip methods.
        The user calls this when they want system-wide action.
        """
        for addr in self.chip_addresses:
            self.disable_all_pixels(addr, power_mode)

    # ==========================================
    # LEVEL 1: CORE PHYSICS (Single Pixel)
    # ==========================================
    def auto_cal_single_pixel(self, addr: int, row: int, col: int) -> list:
        """
        Executes the hardware auto-calibration sequence for a single pixel.
        RETURNS: A dictionary of the exact state for that pixel.
        """
        chip = self._resolve_chip(addr)
        chip.row, chip.col = row, col

        # 1. Hardware Setup
        chip.read_all_block("ETROC2", "Pixel Config")
        setup_vals = {"enable_TDC": 0, "CLKEn_THCal": 1, "BufEn_THCal": 1, "Bypass_THCal": 0}
        for key, val in setup_vals.items():
            chip.set_decoded_value("ETROC2", "Pixel Config", key, val)
        chip.write_all_block("ETROC2", "Pixel Config")

        # 2. Pulse Reset and Start
        for val in [0, 1]:
            chip.set_decoded_value("ETROC2", "Pixel Config", "RSTn_THCal", val)
            chip.write_decoded_value("ETROC2", "Pixel Config", "RSTn_THCal")

        for val in [1, 0]:
            chip.set_decoded_value("ETROC2", "Pixel Config", "ScanStart_THCal", val)
            chip.write_decoded_value("ETROC2", "Pixel Config", "ScanStart_THCal")

        # 3. Safe Polling (Time-bound)
        start_poll = time.monotonic()
        while True:
            chip.read_decoded_value("ETROC2", "Pixel Status", "ScanDone")
            if chip.get_decoded_value("ETROC2", "Pixel Status", "ScanDone") == 1:
                break
            if time.monotonic() - start_poll > 1.0:
                self.logger.error(f"  TIMEOUT: ScanDone not set for ({row},{col}) on {hex(addr)}")
                break
            time.sleep(0.01)

        # 4. Fetch Data
        chip.read_all_block("ETROC2", "Pixel Status")
        bl_nw_data = [
            chip.get_decoded_value("ETROC2", "Pixel Status", "BL"),
            chip.get_decoded_value("ETROC2", "Pixel Status", "NW"),
            datetime.now(),
        ]

        # 5. Cleanup
        cleanup_vals = {"enable_TDC": 0, "CLKEn_THCal": 0, "BufEn_THCal": 0, "Bypass_THCal": 1, "DAC": 0x3ff}
        for key, val in cleanup_vals.items():
            chip.set_decoded_value("ETROC2", "Pixel Config", key, val)
        chip.write_all_block("ETROC2", "Pixel Config")

        return bl_nw_data

    def config_single_pixel(self, addr: int, row: int, col: int, Qsel: int = 0x1e,
                            QInjEn: bool = False, Bypass_THCal: bool = True,
                            power_mode: str = "high", manual_DAC: int = 0x3ff):
        """
        Sets all parameters for a specific pixel in one fast transaction.
        """
        chip = self._resolve_chip(addr)
        chip.row, chip.col = row, col

        power_map = {'high': 0b000, '010': 0b010, '101': 0b101, 'low': 0b111}
        ibsel_val = power_map.get(power_mode, 0b000)

        settings = {
            'disDataReadout': 0, 'QInjEn': 1 if QInjEn else 0, 'disTrigPath': 0,
            'L1Adelay': 0x01f5, 'Bypass_THCal': 1 if Bypass_THCal else 0,
            'TH_offset': 0x14, 'QSel': Qsel, 'DAC': manual_DAC,
            'enable_TDC': 1, 'IBSel': ibsel_val,
        }

        chip.read_all_block("ETROC2", "Pixel Config")

        # Write standard TDC windows (Assume this helper is also adapted to Level 1)
        self.config_TDC_window_ranges_in_memory(addr)

        for key, value in settings.items():
            chip.set_decoded_value("ETROC2", "Pixel Config", key, value)

        chip.write_all_block("ETROC2", "Pixel Config")

    # ==========================================
    # LEVEL 2: BATCH OPERATIONS / ORCHESTRATORS
    # ==========================================
    def batch_auto_calibrate_chip(self, pixel_list: list[tuple]):
        """
        Orchestrates a full 16x16 calibration scan for ONE chip and saves the DataFrame.
        """
        for addr, chip_name in zip(self.chip_addresses, self.chip_names):
            self.logger.info(f'  Starting auto-calibration for {chip_name} ({hex(addr)})...')
            results = {
                'row': [],
                'col': [],
                'baseline': [],
                'noise_width': [],
                'timestamp': [],
            }

            # We can use your preferred tqdm loop here
            for row, col in tqdm(pixel_list, desc=f"Auto baseline calibration for {hex(addr)}"):
                    # Call Level 1 method and capture the dict
                    pixel_data = self.auto_cal_single_pixel(addr, row, col)

                    results['row'].append(row)
                    results['col'].append(col)
                    results['baseline'].append(pixel_data[0])
                    results['noise_width'].append(pixel_data[1])
                    results['timestamp'].append(pixel_data[2])

            # Build DataFrame instantly from the list of dicts
            bl_nw_df = pd.DataFrame(results)
            bl_nw_df['chip_name'] = chip_name

            # Store in the instance state
            self.BL_df[addr] = bl_nw_df
            self.logger.info(f'  Finished auto-calibration for {chip_name} ({hex(addr)})...')

    def batch_enable_pixels(self, pixel_list: list[tuple], Qsel: int = 0x1e,
                            QInjEn: bool = True, Bypass_THCal: bool = True,
                            power_mode: str = "high", offset: int = None, manual_DAC: int = 0x3ff):
        """
        Loops through a provided list of (row, col) tuples and configures them.
        If 'offset' is provided, it automatically grabs the baseline from self.BL_df.
        """
        for addr in self.chip_addresses:
            self.logger.info(f'  Starting pixels configuration for ({hex(addr)})...')
            for row, col in tqdm(pixel_list, desc=f"Configuring Pixels on {hex(addr)}"):

                final_dac = manual_DAC
                # Determine the DAC automatically if offset is requested
                if offset is not None:
                    df = self.BL_df[addr]
                    bl = df.loc[(df['row'] == row) & (df['col'] == col), 'baseline'].values[0]
                    final_dac = bl + offset

                self.config_single_pixel(
                    addr, row, col, Qsel=Qsel, QInjEn=QInjEn,
                    Bypass_THCal=Bypass_THCal, power_mode=power_mode,
                    manual_DAC=final_dac
                )
            self.logger.info(f'  Finished pixels configuration for ({hex(addr)})...')

    # ==========================================
    # LEVEL 1: PERIPHERALS & UTILITIES
    # ==========================================
    def set_chip_peripherals(self, addr: int):
        """Configures the global peripheral registers for a specific chip."""
        try:
            chip = self._resolve_chip(addr)
            chip.read_all_block("ETROC2", "Peripheral Config")

            settings = {
                "EFuse_Prog": 0x00017f0f,         # Chip ID
                "singlePort": 1,                  # Set data output to right port only
                "serRateLeft": 0b00,              # 320 mbps
                "serRateRight": 0b00,             # 320 mbps
                "onChipL1AConf": 0b00,            # Switches off the onboard L1A
                "PLL_ENABLEPLL": 1,               # Debugging use only
                "chargeInjectionDelay": 0x0a,     # User tunable delay of Qinj pulse
                "triggerGranularity": 0x01,       # Only for trigger bit
                "fcClkDelayEn": self.fc_delays[addr]['clk'],
                "fcDataDelayEn": self.fc_delays[addr]['data']
            }

            for key, val in settings.items():
                chip.set_decoded_value("ETROC2", "Peripheral Config", key, val)

            chip.write_all_block("ETROC2", "Peripheral Config")
            self.logger.info(f"  Peripherals set for {hex(addr)}")

        except Exception as e:
            self.logger.error(f"  Failed to set peripherals on {hex(addr)}: {e}")

    def batch_set_chip_peripherals(self):
        """
        A thin wrapper that loops over the single-chip methods.
        The user calls this when they want system-wide action.
        """
        for addr in self.chip_addresses:
            self.set_chip_peripherals(addr)

    def calibratePLL(self, addr: int):
        """Hardware sequence to reset and lock the PLL."""
        chip = self._resolve_chip(addr)

        # 1. PLL Reset (Toggle 0 -> 1)
        chip.read_decoded_value("ETROC2", "Peripheral Config", 'asyPLLReset')
        for val in [0, 1]:
            chip.set_decoded_value("ETROC2", "Peripheral Config", 'asyPLLReset', val)
            chip.write_decoded_value("ETROC2", "Peripheral Config", 'asyPLLReset')
            time.sleep(0.1)

        # 2. Start Calibration (Toggle 0 -> 1)
        chip.read_decoded_value("ETROC2", "Peripheral Config", 'asyStartCalibration')
        for val in [0, 1]:
            chip.set_decoded_value("ETROC2", "Peripheral Config", 'asyStartCalibration', val)
            chip.write_decoded_value("ETROC2", "Peripheral Config", 'asyStartCalibration')
            time.sleep(0.1)

    def asyAlignFastcommand(self, addr: int):
        """Hardware sequence to align the Fast Command decoder."""
        chip = self._resolve_chip(addr)
        chip.read_decoded_value("ETROC2", "Peripheral Config", 'asyAlignFastcommand')

        # Toggle 1 -> 0
        for val in [1, 0]:
            chip.set_decoded_value("ETROC2", "Peripheral Config", 'asyAlignFastcommand', val)
            chip.write_decoded_value("ETROC2", "Peripheral Config", 'asyAlignFastcommand')
            if val == 1: time.sleep(0.1)

    def asyResetGlobalReadout(self, addr: int):
        """Hardware sequence to reset the global readout block."""
        chip = self._resolve_chip(addr)
        chip.read_decoded_value("ETROC2", "Peripheral Config", 'asyResetGlobalReadout')

        # Toggle 0 -> 1
        for val in [0, 1]:
            chip.set_decoded_value("ETROC2", "Peripheral Config", 'asyResetGlobalReadout', val)
            chip.write_decoded_value("ETROC2", "Peripheral Config", 'asyResetGlobalReadout')
            if val == 0: time.sleep(0.1)

    def batch_PLL_FC_calibration(self):
        """
        A thin wrapper that loops over the single-chip methods.
        The user calls this when they want system-wide action.
        """
        for addr in self.chip_addresses:
            self.calibratePLL(addr)
            self.asyResetGlobalReadout(addr)
            self.asyAlignFastcommand(addr)

    def config_fc_data_delay(self, addr: int, fc_clk_delay: int, fc_data_delay: int):
        """Updates the Fast Command delays in memory and on the chip."""
        if not (0 <= fc_clk_delay <= 1) or not (0 <= fc_data_delay <= 1):
            raise ValueError('Delays must be 0 or 1')

        self.fc_delays[addr]['clk'] = fc_clk_delay
        self.fc_delays[addr]['data'] = fc_data_delay

        chip = self._resolve_chip(addr)
        chip.read_register("ETROC2", "Peripheral Config", "PeriCfg18")
        chip.set_decoded_value("ETROC2", "Peripheral Config", "fcClkDelayEn", fc_clk_delay)
        chip.set_decoded_value("ETROC2", "Peripheral Config", "fcDataDelayEn", fc_data_delay)
        chip.write_register("ETROC2", "Peripheral Config", "PeriCfg18")


    # ==========================================
    # LEVEL 1: DIAGNOSTIC & CHECK METHODS
    # ==========================================
    def pixel_check(self, addr: int) -> bool:
        """
        Verifies the consistency of the PixelID (Row/Col) for the entire 16x16 grid.
        """
        chip = self._resolve_chip(addr)
        pixel_flag_fail = False

        try:
            for row in range(16):
                for col in range(16):
                    chip.row, chip.col = row, col

                    # Read and verify PixelID consistency
                    chip.read_decoded_value("ETROC2", "Pixel Status", 'PixelID')
                    fetched_row = chip.get_decoded_value("ETROC2", "Pixel Status", 'PixelID-Row')
                    fetched_col = chip.get_decoded_value("ETROC2", "Pixel Status", 'PixelID-Col')

                    if row != fetched_row or col != fetched_col:
                        self.logger.error(f"  {hex(addr)}: Pixel ({row}, {col}) failed consistency check! Returned ({fetched_row}, {fetched_col})")
                        pixel_flag_fail = True

            return not pixel_flag_fail

        except Exception as e:
            self.logger.error(f"  Error in pixel_check for {hex(addr)}: {e}")
            return False

    # --------------------------------------------------------------------------
    def basic_peripheral_register_check(self, addr: int) -> bool:
        """
        Performs a bit-flip (XOR 0xFF) test on all 32 peripheral configuration registers
        to verify write/read persistence and consistency.
        """
        chip = self._resolve_chip(addr)
        peri_flag_fail = False

        try:
            # Initial bulk read of the block
            chip.read_all_block("ETROC2", "Peripheral Config")

            for i in range(32):
                reg_name = f"PeriCfg{i}"

                # 1. Fetch original and create bit-flipped version
                original_val = chip["ETROC2", "Peripheral Config", reg_name]
                modified_val = original_val ^ 0xff

                # 2. Write flipped value and verify immediately
                chip["ETROC2", "Peripheral Config", reg_name] = modified_val
                chip.write_register("ETROC2", "Peripheral Config", reg_name)

                # Internal read-back check
                read_1 = chip["ETROC2", "Peripheral Config", reg_name]
                chip.read_register("ETROC2", "Peripheral Config", reg_name)
                read_2 = chip["ETROC2", "Peripheral Config", reg_name]

                # 3. Restore original value and verify
                chip["ETROC2", "Peripheral Config", reg_name] = original_val
                chip.write_register("ETROC2", "Peripheral Config", reg_name)
                restored_val = chip["ETROC2", "Peripheral Config", reg_name]

                # Validation Logic
                if read_1 != read_2 or read_2 != modified_val or restored_val != original_val:
                    self.logger.error(f"  {hex(addr)}: {reg_name} bit-flip check FAILURE")
                    peri_flag_fail = True

            return not peri_flag_fail

        except Exception as e:
            self.logger.error(f"  Error in peripheral_check for {hex(addr)}: {e}")
            return False

    # ==========================================
    # LEVEL 2: BATCH DIAGNOSTICS
    # ==========================================
    def batch_pixel_check(self) -> bool:
        """Runs the PixelID consistency check on all registered chips."""
        for addr in self.chip_addresses:
            success = self.pixel_check(addr)
            status = f"{GREEN}PASSED{RESET}" if success else f"{RED}FAILED{RESET}"
            self.logger.info(f"  System Check: PixelID consistency for {hex(addr)}: {status}")

    def batch_peripheral_check(self) -> bool:
        """Runs the bit-flip register check on all registered chips."""
        for addr in self.chip_addresses:
            success = self.basic_peripheral_register_check(addr)
            status = f"{GREEN}PASSED{RESET}" if success else f"{RED}FAILED{RESET}"
            self.logger.info(f"  System Check: Peripheral registers for {hex(addr)}: {status}")

    def check_invalid_fc(self, addr: int, samples: int = 3, interval: float = 0.3) -> list[int]:
        """
        Reads the invalid FCCount register multiple times to check for decoding stability.
        Returns a list of the counter values found.
        """
        chip = self._resolve_chip(addr)
        invalid_fc_values = []

        for _ in range(samples):
            # Read and fetch the decoded counter
            chip.read_decoded_value("ETROC2", "Peripheral Status", 'invalidFCCount')
            val = chip.get_decoded_value("ETROC2", "Peripheral Status", "invalidFCCount")
            invalid_fc_values.append(val)
            time.sleep(interval)

        return invalid_fc_values

    def batch_check_invalid_fc(self):
        """
        System-wide check of Invalid FC counters for all registered chips.
        """
        self.logger.info("  Checking Invalid FC counters across all chips...")

        for addr in self.chip_addresses:
            fc_values = self.check_invalid_fc(addr)
            self.logger.info(f"  Chip {hex(addr)} Invalid FC Counter: {fc_values}")