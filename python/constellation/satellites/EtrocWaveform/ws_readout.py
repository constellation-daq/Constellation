import i2c_gui2
import logging
import sys
import pandas as pd
import numpy as np

class i2c_connection():
    _chips = None

    def __init__(self, port, chip_addresses, ws_addresses, chip_names, clock = 100):
        self.chip_addresses = chip_addresses
        self.ws_addresses = ws_addresses
        self.chip_names = chip_names

        ## Logger
        log_level = 30
        logging.basicConfig(format='%(asctime)s - %(levelname)s:%(name)s:%(message)s', stream=sys.stdout, force=False, level=log_level)
        # logger = logging.getLogger("Script_Logger")
        self.chip_logger = logging.getLogger("Chip_Logger")
        self.conn = i2c_gui2.USB_ISS_Helper(port, clock, dummy_connect = False)
        # logger.setLevel(log_level)
        self.chip_logger.setLevel(log_level)

    def __del__(self):
        del self.conn

    def get_chip_i2c_connection(self, chip_address, ws_address=None):
        if self._chips is None:
            self._chips = {}

        if chip_address not in self._chips:
            self._chips[chip_address] = i2c_gui2.ETROC2_Chip(chip_address, ws_address, self.conn, self.chip_logger)

        return self._chips[chip_address]

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