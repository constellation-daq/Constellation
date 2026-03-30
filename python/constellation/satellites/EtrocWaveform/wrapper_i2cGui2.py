import numpy as np
import pandas as pd

import i2c_gui2
import logging
import time

from datetime import datetime

GREEN = '\033[32m'  # ANSI code for green text
RED   = "\033[31m"  # ANSI code for red text
RESET = '\033[0m'   # ANSI code to reset text formatting

class i2c_connection():
    def __init__(self, port, chip_addresses, ws_addresses, chip_names, clock=100):
        self.chip_addresses = chip_addresses
        self.ws_addresses = ws_addresses
        self.chip_names = chip_names

        # OPTIMIZATION: Dictionary comprehensions make this instantly readable
        self.fc_clk_delay = {addr: 1 for addr in chip_addresses}
        self.fc_data_delay = {addr: 1 for addr in chip_addresses}

        # OPTIMIZATION: Instance variable prevents cross-contamination if multiple USBs are used
        self._chips = {}

        ## Logger Setup
        log_level = 50
        self.chip_logger = logging.getLogger("Chip_Logger")
        self.chip_logger.setLevel(log_level)
        self.chip_logger.propagate = False

        self.i2c_logger = logging.getLogger("I2C_Log")
        self.i2c_logger.setLevel(log_level)
        self.i2c_logger.propagate = False

        if not self.chip_logger.handlers:
            self.chip_logger.addHandler(logging.NullHandler())
        if not self.i2c_logger.handlers:
            self.i2c_logger.addHandler(logging.NullHandler())

        self.conn = i2c_gui2.USB_ISS_Helper(port, clock, dummy_connect=False)

    def __del__(self):
        # Safety check in case conn fails to initialize
        if hasattr(self, 'conn'):
            del self.conn

    def _resolve_chip(self, chip_address: int = None, chip: i2c_gui2.ETROC2_Chip = None, ws_address: int = None) -> i2c_gui2.ETROC2_Chip:
        """
        Master helper to fetch or create a chip connection.
        Completely eliminates the 'if chip == None' boilerplate across the class.
        """
        if chip is not None:
            return chip

        if chip_address is None:
            raise ValueError("CRITICAL: Must provide either a 'chip' object or a 'chip_address'!")

        if chip_address not in self._chips:
            self._chips[chip_address] = i2c_gui2.ETROC2_Chip(chip_address, ws_address, self.conn, self.chip_logger)

        return self._chips[chip_address]

    #--------------------------------------------------------------------------#
    def auto_cal_single_pixel(self, chip_address: int = None, row: int = 0, col: int = 0, chip: i2c_gui2.ETROC2_Chip=None):

        # ONE LINE replaces the 5-line if/elif block!
        chip = self._resolve_chip(chip_address, chip)

        chip.row = row
        chip.col = col

        chip.read_all_block("ETROC2", "Pixel Config")

        # Disable TDC, Enable THCal clock and buffer, disable bypass
        chip.set_decoded_value("ETROC2", "Pixel Config", "enable_TDC", 0)
        chip.set_decoded_value("ETROC2", "Pixel Config", "CLKEn_THCal", 1)
        chip.set_decoded_value("ETROC2", "Pixel Config", "BufEn_THCal", 1)
        chip.set_decoded_value("ETROC2", "Pixel Config", "Bypass_THCal", 0)
        chip.write_all_block("ETROC2", "Pixel Config")

        # Reset the calibration block (active low)
        chip.set_decoded_value("ETROC2", "Pixel Config", "RSTn_THCal", 0)
        chip.write_decoded_value("ETROC2", "Pixel Config", "RSTn_THCal")
        chip.set_decoded_value("ETROC2", "Pixel Config", "RSTn_THCal", 1)
        chip.write_decoded_value("ETROC2", "Pixel Config", "RSTn_THCal")

        # Start and Stop the calibration, (25ns x 2**15 ~ 800 us, ACCumulator max is 2**15)
        chip.set_decoded_value("ETROC2", "Pixel Config", "ScanStart_THCal", 1)
        chip.write_decoded_value("ETROC2", "Pixel Config", "ScanStart_THCal")
        chip.set_decoded_value("ETROC2", "Pixel Config", "ScanStart_THCal", 0)
        chip.write_decoded_value("ETROC2", "Pixel Config", "ScanStart_THCal")

        # Wait for the calibration to be done correctly
        retry_counter = 0
        chip.read_decoded_value("ETROC2", "Pixel Status", "ScanDone")
        while chip.get_decoded_value("ETROC2", "Pixel Status", "ScanDone") != 1:
            time.sleep(0.01)
            chip.read_decoded_value("ETROC2", "Pixel Status", "ScanDone")
            retry_counter += 1
            if retry_counter == 100:
                print(f"Retry counter reaches at 100! // Auto_Calibration Scan has failed for row {row}, col {col}!!")
                break

        chip.read_all_block("ETROC2", "Pixel Status")

        # Save outputs
        bl_nw_output = {
            "row": row,
            "col": col,
            "baseline": chip.get_decoded_value("ETROC2", "Pixel Status", "BL"),
            "noise_width": chip.get_decoded_value("ETROC2", "Pixel Status", "NW"),
            "timestamp": datetime.now(),
        }

        # Disable THCal, Enable bypass, set DAC
        chip.set_decoded_value("ETROC2", "Pixel Config", "enable_TDC", 0)
        chip.set_decoded_value("ETROC2", "Pixel Config", "CLKEn_THCal", 0)
        chip.set_decoded_value("ETROC2", "Pixel Config", "BufEn_THCal", 0)
        chip.set_decoded_value("ETROC2", "Pixel Config", "Bypass_THCal", 1)
        chip.set_decoded_value("ETROC2", "Pixel Config", "DAC", 0x3ff)
        chip.write_all_block("ETROC2", "Pixel Config")

        return bl_nw_output

    #--------------------------------------------------------------------------#
    def config_TDC_window_ranges_in_memory(self, chip: i2c_gui2.ETROC2_Chip, window_dict: dict = None):
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

    #--------------------------------------------------------------------------#
    def config_single_pixel(self, chip_address: int, row: int, col: int, Qsel: int=None,
                            QInjEn: bool=False, Bypass_THCal: bool=True, power_mode: str="high",
                            baseline: int=500, offset: int=None, manual_DAC: int=0x3ff,
                            chip: i2c_gui2.ETROC2_Chip=None):

        chip = self._resolve_chip(chip_address, chip)
        chip.row, chip.col = row, col

        # 1. Determine the final DAC value
        final_dac = manual_DAC
        if offset is not None:
            final_dac = baseline + offset

        # 2. Map Power Mode
        power_map = {'high': 0b000, '010': 0b010, '101': 0b101, 'low': 0b111}
        ibsel_val = power_map.get(power_mode, 0b000)

        # 3. Build Configuration Dictionary
        pixel_config_dict = {
            'disDataReadout': 0,
            'QInjEn': 1 if QInjEn else 0,
            'disTrigPath': 0,
            'L1Adelay': 0x01f5,
            'Bypass_THCal': 1 if Bypass_THCal else 0,
            'TH_offset': 0x14,
            'QSel': Qsel if Qsel is not None else 0x1e,
            'DAC': final_dac, # Set the calculated or manual threshold here
            'enable_TDC': 1,
            'IBSel': ibsel_val,
        }

        # 4. Write to Memory and Hardware
        chip.read_all_block("ETROC2", "Pixel Config")
        self.config_TDC_window_ranges_in_memory(chip=chip)

        for key, value in pixel_config_dict.items():
            chip.set_decoded_value("ETROC2", "Pixel Config", key, value)

        chip.write_all_block("ETROC2", "Pixel Config")

    #--------------------------------------------------------------------------#
    def config_power_mode(self, chip_address: int, scan_list: list[tuple], power_mode: str='high'):

        chip = self._resolve_chip(chip_address)

        power_map = {'high': 0b000, '010': 0b010, '101': 0b101, 'low': 0b111}
        ibsel_val = power_map.get(power_mode, 0b000)

        for row, col in scan_list:
            chip.row = row
            chip.col = col
            chip.set_decoded_value("ETROC2", "Pixel Config", "IBSel", ibsel_val)
            chip.write_decoded_value("ETROC2", "Pixel Config", "IBSel")

    #--------------------------------------------------------------------------#
    def config_fc_data_delay(self, chip_address: int, fc_clk_delay: int, fc_data_delay: int):
        if fc_clk_delay not in (0, 1): raise ValueError('fc_clk_delay value must be 0 or 1')
        if fc_data_delay not in (0, 1): raise ValueError('fc_data_delay value must be 0 or 1')

        self.fc_clk_delay[chip_address] = fc_clk_delay
        self.fc_data_delay[chip_address] = fc_data_delay

        chip = self._resolve_chip(chip_address)

        chip.read_register("ETROC2", "Peripheral Config", "PeriCfg18")
        chip.set_decoded_value("ETROC2", "Peripheral Config", "fcClkDelayEn", fc_clk_delay)
        chip.set_decoded_value("ETROC2", "Peripheral Config", "fcDataDelayEn", fc_data_delay)
        chip.write_register("ETROC2", "Peripheral Config", "PeriCfg18")

        print(f"FC delays has been changed for the chip: {hex(chip_address)}")

    #--------------------------------------------------------------------------#
    def set_chip_peripherals(self, chip_address=None, chip: i2c_gui2.ETROC2_Chip=None):
        try:
            chip = self._resolve_chip(chip_address, chip)

            chip.read_all_block("ETROC2", "Peripheral Config")
            chip.set_decoded_value("ETROC2", "Peripheral Config", "EFuse_Prog", 0x00017f0f)
            chip.set_decoded_value("ETROC2", "Peripheral Config", "singlePort", 1)
            chip.set_decoded_value("ETROC2", "Peripheral Config", "serRateLeft", 0b00)
            chip.set_decoded_value("ETROC2", "Peripheral Config", "serRateRight", 0b00)
            chip.set_decoded_value("ETROC2", "Peripheral Config", "onChipL1AConf", 0b00)
            chip.set_decoded_value("ETROC2", "Peripheral Config", "PLL_ENABLEPLL", 1)
            chip.set_decoded_value("ETROC2", "Peripheral Config", "chargeInjectionDelay", 0x0a)
            chip.set_decoded_value("ETROC2", "Peripheral Config", "triggerGranularity", 0x01)
            chip.set_decoded_value("ETROC2", "Peripheral Config", "fcClkDelayEn", self.fc_clk_delay[chip_address])
            chip.set_decoded_value("ETROC2", "Peripheral Config", "fcDataDelayEn", self.fc_data_delay[chip_address])
            chip.write_all_block("ETROC2", "Peripheral Config")

        except Exception as e:
            print(f"{RED}An error occurred in set_chip_peripherals: {e}{RESET}")

    #--------------------------------------------------------------------------#
    def disable_all_pixels(self, chip_address=None, power_mode='high', chip: i2c_gui2.ETROC2_Chip=None):
        chip = self._resolve_chip(chip_address, chip)

        chip.row = 0
        chip.col = 0
        chip.read_all_block("ETROC2", "Pixel Config")

        power_map = {'high': 0b000, '010': 0b010, '101': 0b101, 'low': 0b111}
        ibsel_val = power_map.get(power_mode, 0b000)

        pixel_config = {
            "disDataReadout": 1, "QInjEn": 0, "disTrigPath": 1,
            "upperTOATrig": 0x000, "lowerTOATrig": 0x000,
            "upperTOTTrig": 0x1ff, "lowerTOTTrig": 0x1ff,
            "upperCalTrig": 0x3ff, "lowerCalTrig": 0x3ff,
            "upperTOA": 0x000, "lowerTOA": 0x000,
            "upperTOT": 0x1ff, "lowerTOT": 0x1ff,
            "upperCal": 0x3ff, "lowerCal": 0x3ff,
            "enable_TDC": 0,
            "IBSel": ibsel_val,
            "Bypass_THCal": 1,
            "TH_offset": 0x3f,
            "DAC": 0x3ff,
        }

        for key, value in pixel_config.items():
            chip.set_decoded_value("ETROC2", "Pixel Config", key, value)

        chip.broadcast = True
        chip.write_all_block("ETROC2", "Pixel Config")
        chip.broadcast = False

    #--------------------------------------------------------------------------#
    # FAST COMMAND HELPERS
    #--------------------------------------------------------------------------#
    def asyAlignFastcommand(self, chip_address=None, chip: i2c_gui2.ETROC2_Chip=None):
        chip = self._resolve_chip(chip_address, chip)
        chip.read_decoded_value("ETROC2", "Peripheral Config", 'asyAlignFastcommand')
        chip.set_decoded_value("ETROC2", "Peripheral Config", 'asyAlignFastcommand', 1)
        chip.write_decoded_value("ETROC2", "Peripheral Config", 'asyAlignFastcommand')
        time.sleep(0.1)
        chip.set_decoded_value("ETROC2", "Peripheral Config", 'asyAlignFastcommand', 0)
        chip.write_decoded_value("ETROC2", "Peripheral Config", 'asyAlignFastcommand')

    def asyResetGlobalReadout(self, chip_address=None, chip: i2c_gui2.ETROC2_Chip=None):
        chip = self._resolve_chip(chip_address, chip)
        chip.read_decoded_value("ETROC2", "Peripheral Config", 'asyResetGlobalReadout')
        chip.set_decoded_value("ETROC2", "Peripheral Config", 'asyResetGlobalReadout', 0)
        chip.write_decoded_value("ETROC2", "Peripheral Config", 'asyResetGlobalReadout')
        time.sleep(0.1)
        chip.set_decoded_value("ETROC2", "Peripheral Config", 'asyResetGlobalReadout', 1)
        chip.write_decoded_value("ETROC2", "Peripheral Config", 'asyResetGlobalReadout')

    def calibratePLL(self, chip_address=None, chip: i2c_gui2.ETROC2_Chip=None):
        chip = self._resolve_chip(chip_address, chip)

        chip.read_decoded_value("ETROC2", "Peripheral Config", 'asyPLLReset')
        chip.set_decoded_value("ETROC2", "Peripheral Config", 'asyPLLReset', 0)
        chip.write_decoded_value("ETROC2", "Peripheral Config", 'asyPLLReset')
        time.sleep(0.1)
        chip.set_decoded_value("ETROC2", "Peripheral Config", 'asyPLLReset', 1)
        chip.write_decoded_value("ETROC2", "Peripheral Config", 'asyPLLReset')

        chip.read_decoded_value("ETROC2", "Peripheral Config", 'asyStartCalibration')
        chip.set_decoded_value("ETROC2", "Peripheral Config", 'asyStartCalibration', 0)
        chip.write_decoded_value("ETROC2", "Peripheral Config", 'asyStartCalibration')
        time.sleep(0.1)
        chip.set_decoded_value("ETROC2", "Peripheral Config", 'asyStartCalibration', 1)
        chip.write_decoded_value("ETROC2", "Peripheral Config", 'asyStartCalibration')

    #--------------------------------------------------------------------------#
    def start_ws_sampling(self):
        for chip_address,ws_address in zip(self.chip_addresses,self.ws_addresses):
            chip: i2c_gui2.ETROC2_Chip = self.get_chip_i2c_connection(chip_address, ws_address)

            chip.read_register("Waveform Sampler", "Config", "regOut1F")
            chip["Waveform Sampler", "Config", "regOut1F"] = 0x22
            chip.write_register("Waveform Sampler", "Config", "regOut1F")
            chip["Waveform Sampler", "Config", "regOut1F"] = 0x0b
            chip.write_register("Waveform Sampler", "Config", "regOut1F")

            chip.read_decoded_value("Waveform Sampler", "Config", 'mem_rstn')
            chip.set_decoded_value("Waveform Sampler", "Config", 'mem_rstn', 0)
            chip.write_decoded_value("Waveform Sampler", "Config", 'mem_rstn')
            chip.set_decoded_value("Waveform Sampler", "Config", 'mem_rstn', 1)
            chip.write_decoded_value("Waveform Sampler", "Config", 'mem_rstn')

            chip.read_decoded_value("Waveform Sampler", "Config", 'DDT')
            chip.set_decoded_value("Waveform Sampler", "Config", 'DDT', 0)        # Time Skew Calibration set to 0
            chip.write_decoded_value("Waveform Sampler", "Config", 'DDT')

            chip.read_register("Waveform Sampler", "Config", "regOut0D")
            chip.set_decoded_value("Waveform Sampler", "Config", 'CTRL', 2)       # CTRL default = 0x10 for regOut0D
            chip.write_decoded_value("Waveform Sampler", "Config", 'CTRL')
            chip.set_decoded_value("Waveform Sampler", "Config", 'comp_cali', 0)       # Comparator calibration should be off
            chip.write_decoded_value("Waveform Sampler", "Config", 'comp_cali')

    def stop_ws_sampling(self):
        for chip_address,ws_address in zip(self.chip_addresses,self.ws_addresses):
            chip: i2c_gui2.ETROC2_Chip = self.get_chip_i2c_connection(chip_address, ws_address)
            chip.read_register("Waveform Sampler", "Config", "regOut1F")
            chip["Waveform Sampler", "Config", "regOut1F"] = 0x09
            chip.write_register("Waveform Sampler", "Config", "regOut1F")


    def read_chip_ws(self, chip_address, ws_address):
        chip: i2c_gui2.ETROC2_Chip = self.get_chip_i2c_connection(chip_address, ws_address)
        i2c_controller: i2c_gui2.I2C_Connection_Helper = chip._i2c_connection
        chip.read_decoded_value("Waveform Sampler", "Config", 'rd_en_I2C')
        chip.set_decoded_value("Waveform Sampler", "Config", 'rd_en_I2C', 1)
        chip.write_decoded_value("Waveform Sampler", "Config", 'rd_en_I2C')

        max_steps = 1024
        base_data = []
        coeff = 0.085 ## 0.05/5 * 8.5
        time_coeff = 0.390625 ## 1/2.56
        addr_regs = [0x00, 0x00]

        for address in range(max_steps):
            addr_regs[0] = ((address & 0b11) << 6)
            addr_regs[1] = ((address & 0b1111111100) >> 2)
            i2c_controller.write_device_memory(ws_address, 0x1C, addr_regs, 8)
            tmp_data = i2c_controller.read_device_memory(ws_address, 0x20, 2, 8)

            # --- OPTIMIZATION: PURE BITWISE PARSING ---
            # 1. Reconstruct the 14-bit integer instantly
            data_int = (tmp_data[0] >> 2) | (tmp_data[1] << 6)

            # 2. Extract bits natively instead of using string slicing
            pointer = (data_int >> 13) & 1               # The 14th bit
            Dout_S1 = (data_int >> 7) & 0x3F             # The next 6 bits (bits 7 to 12)

            # 3. Apply your custom ADC multipliers to the lowest 7 bits directly
            Dout_S2 = (((data_int >> 6) & 1) * 24 +
                       ((data_int >> 5) & 1) * 16 +
                       ((data_int >> 4) & 1) * 10 +
                       ((data_int >> 3) & 1) * 6 +
                       ((data_int >> 2) & 1) * 4 +
                       ((data_int >> 1) & 1) * 2 +
                        (data_int & 1))

            base_data.append(
                {
                    "Data Address": address,
                    "Data": data_int,
                    "Raw Data": f"{data_int:014b}", # Instant zero-padded binary string
                    "pointer": pointer,
                    "Dout_S1": Dout_S1,
                    "Dout_S2": Dout_S2,
                    "Dout": Dout_S1 - (coeff * Dout_S2),
                }
            )

        # 1. Create the DataFrame and calculate chunk sizes
        df = pd.DataFrame(base_data)
        channels = 8
        steps_per_ch = len(df) // channels

        # 2. Vectorized Assignment: Assign 'Channel' and 'Step' to every row instantly
        # 'Channel' becomes [1...1, 2...2, ..., 8...8]
        # 'Step' becomes [0...127, 0...127, ..., 0...127]
        df['Channel'] = np.repeat(np.arange(1, channels + 1), steps_per_ch)
        df['Step'] = np.tile(np.arange(steps_per_ch), channels)

        # 3. Find the pointer in the last channel (Channel 8)
        pointer_mask = (df['Channel'] == channels) & (df['pointer'] != 0)

        if pointer_mask.any():
            # Get the step index where the pointer was found
            pointer_step = df.loc[pointer_mask, 'Step'].iloc[0]

            # 4. Circular Shift (Roll) using modulo math instead of set.difference()
            # This perfectly shifts the order for ALL channels simultaneously
            df['Step'] = (df['Step'] - (pointer_step + 1)) % steps_per_ch

        # 5. Vectorized Interleaving: Calculate Time Index for all rows instantly
        df['Time Index'] = df['Step'] * channels + (channels - df['Channel'])
        df['Time [ns]'] = df['Time Index'] * time_coeff

        # 6. Sort by our new index and clean up temporary columns
        df = df.sort_values('Time Index').set_index('Time Index')
        df = df.drop(columns=['Step'])

        # --- I2C Cleanup ---
        chip.read_decoded_value("Waveform Sampler", "Config", 'rd_en_I2C')
        chip.set_decoded_value("Waveform Sampler", "Config", 'rd_en_I2C', 0)        #active high
        chip.write_decoded_value("Waveform Sampler", "Config", 'rd_en_I2C')

        return df