#!/usr/bin/env python
# -*- coding: utf-8 -*-
import struct
import warnings

# ------------------------------------------------------------------------
# Module-level Constants / Lookups
# Moved outside functions to prevent recreation on every function call.
# ------------------------------------------------------------------------

#--------------------------------------------------------------------------#
# Following Info is current for commit https://github.com/CMS-ETROC/ETROC2TestFirmware/commit/e40cb281b88d957c9dea2cbba983f1004d6ffce2
#--------------------------------------------------------------------------#
# Register [15:0] Configuration Map:
# -------------------------------------------------------------------------
# Bits    | Name                        | Function
# -------------------------------------------------------------------------
# [15:12] | RESERVED                    | Unused
# [11]    | start_phase_detect          | Phase detection enable
# [10]    | stop_DAQ_pulse              | Stop Data Acquisition
# [9]     | start_DAQ_pulse             | Start Data Acquisition
# [8]     | start_hist_counter          | Histogram counter enable
# [7]     | resumePulse                 | Resume pulse trigger
# [6]     | clear_ws_trig_block_pulse   | Clear wait-state trigger block
# [5]     | clrError                    | Error flag reset
# [4]     | initPulse                   | System initialization pulse
# [3]     | errInjPulse                 | Error injection (Test mode)
# [2]     | fcStart                     | Flow Control / Freq Start
# [1]     | fifo_reset                  | FIFO buffer reset
# [0]     | START                       | Master Start
# -------------------------------------------------------------------------

PULSE_REGISTERS = {
    "clear_fifo": 0x0002,
    "fc_signal_start": 0x0004,
    "err_inj": 0x0008,
    "fc_init": 0x0010,
    "reset_counter": 0x0020,
    "clear_ws_block": 0x0040,
    "resume_in_debug": 0x0080,
    "start_hist_counter": 0x0100,
    "start_DAQ": 0x0200,
    "stop_DAQ": 0x0400,
    "start_phase_detect": 0x0800,
}

#--------------------------------------------------------------------------#
# Config Register:
# Reg 4 : {WR_ADDR[7:0],WR_DATA0[7:0]} //I2C
# Reg 5 : {6'bxxxxxx,MODE[1:0],SL_ADDR[6:0],SL_WR} //I2C
# Reg 6 : {8'bxxxxxxxx, WR_DATA1[7:0]} //I2C
# Reg 7 : {6'bxxxxxx,delayTrigCh[3:0],3'bxxx,dis_descr_raw_data,dis_regular_filler, inject_SEU} //trigbit delay or not
# Reg 8 : {trigSelMask[3:0],enhenceData,enableL1Trig,L1Delay[9:0]}
# Reg 9 : {4'bxxxx, initAddressLast[11:0]}
# Reg 10 : {prescale_factor,initAddressFirst[11:0]}
# Reg 11 : {duration[15:0]} \ Reg 12 : {errorMask[7:0],trigDataSize[1:0],period,1'bx,inputCmd[3:0]}
# Reg 13 : {5'bxxxxx, data_delay[5:0],dataRate[1:0],LED_Pages[2:0],status_Pages[1:0]}
# Reg 14 : {auto_prescale,fixed_time_filler,4'bxxxx,falling_edge,manual_mode,sample_event,simple_handshake,add_ethernet_filler,debug_mode,dumping_mode,notGTXPolarity,notGTX,enableAutoSync}
# Reg 15 : {global_trig_delay[4:0],global_trig,trig_or_logic,triple_trig,en_ws_trig,ws_trig_stop_delay[2:0],enableCh[3:0]}

CONFIG_REGISTERS = {
    "fc_delays": 4,
    "data_delays_01": 5,
    "data_delays_23": 6,
    "counter_duration": 7,
    "triggerbit_delay": 8,
    "register_9": 9,
    "register_10": 10,
    "register_11": 11,
    "register_12": 12,
    "timestamp": 13,
    "polarity": 14,
    "active_channel": 15,
}

VALID_PRESCALE_FACTORS = {
    2048: 0b00,
    4096: 0b01,
    8192: 0b10,
    16384: 0b11,
}

# ------------------------------------------------------------------------
# Helper Functions
# ------------------------------------------------------------------------

def _recv_exact(sock, num_bytes):
    """Helper to ensure exactly num_bytes are received from the socket."""
    data = bytearray()
    while len(data) < num_bytes:
        packet = sock.recv(num_bytes - len(data))
        if not packet:
            return None # Connection closed
        data.extend(packet)
    return data

# ------------------------------------------------------------------------
# Communication Functions
# ------------------------------------------------------------------------

## write config_reg
# @param[in] Addr Address of the configuration register 0-31
# @param[in] Data write into the configuration register 0-65535, [15:0]
def write_config_reg(ss, Addr, Data):
    data = 0x00200000 + (Addr << 16) + Data
    ss.sendall(struct.pack('>I', data))

## read config_reg
# @param[in] Addr Address of the configuration register 0-31
# return 32bit data
def read_config_reg(ss, Addr):
    data = 0x80200000 + (Addr << 16)
    ss.sendall(struct.pack('>I', data))
    return struct.unpack('>I', _recv_exact(ss, 4))[0]

## write pulse_reg
# @param[in] Data write into the pulse register 0-65535
def write_pulse_reg(ss, Data):
    data = 0x000b0000 + Data
    ss.sendall(struct.pack('>I', data))

## read status_reg
# @param[in] Addr Address of the configuration register 0-10
def read_status_reg(ss, Addr):
    data = 0x80000000 + (Addr << 16)
    ss.sendall(struct.pack('>I', data))
    return struct.unpack('>I', _recv_exact(ss, 4))[0]

## write memeoy
# @param[in] Addr write address of memeoy 0-65535
# @param[in] Data write into memory data 0-65535
def write_memory(ss, Addr, Data):
    # Pack into a single byte string before sending to reduce network calls
    payload = struct.pack('>4I',
        0x00110000 + (0x0000ffff & Addr),              # memory address LSB register
        0x00120000 + ((0xffff0000 & Addr) >> 16),      # memory address MSB register
        0x00130000 + (0x0000ffff & Data),              # memory Data LSB register
        0x00140000 + ((0xffff0000 & Data) >> 16)       # memory Data MSB register
    )
    ss.sendall(payload)

## read memory
# @param[in] Cnt read data counts 0-65535
# @param[in] Addr start address of read memory 0-65535
def read_memory(ss, Cnt, Addr):
    payload = struct.pack('>4I',
        0x00100000 + Cnt,                             # write sMemioCnt
        0x00110000 + (0x0000ffff & Addr),             # write memory address LSB register
        0x00120000 + ((0xffff0000 & Addr) >> 16),     # write memory address MSB register
        0x80140000                                    # read Cnt 32bit memory words
    )
    ss.sendall(payload)

    # Bulk read to avoid iterating individual 4-byte receives
    raw_data = _recv_exact(ss, Cnt * 4)
    if raw_data:
        unpacked_data = struct.unpack(f'>{Cnt}I', raw_data)
        for val in unpacked_data:
            print(hex(val))

## read_data_fifo
# @param[in] Cnt read data counts 0-65535
def read_data_fifo(ss, Cnt):
    data = 0x00190000 + (Cnt - 1)
    ss.sendall(struct.pack('>I', data))

    # Replaced loop with a single bulk read and bulk unpack for massive speedup
    expected_bytes = Cnt * 4
    raw_data = _recv_exact(ss, expected_bytes)

    if not raw_data:
        warnings.warn("Socket closed before data could be received.")
        return b""  # <-- Changed to empty bytes

    if len(raw_data) < expected_bytes:
            warnings.warn(f"Only received {len(raw_data)} out of {expected_bytes} bytes.")
            valid_bytes = (len(raw_data) // 4) * 4
            return raw_data[:valid_bytes] # Return truncated raw bytes

    return raw_data # Return the raw bytes directly!

def write_pulse_reg_decoded(ss, key=""):
    if key not in PULSE_REGISTERS:
        raise RuntimeError(f"Invalid Pulse Register Key given: {key}")
    write_pulse_reg(ss, PULSE_REGISTERS[key])

def write_config_reg_decoded(ss, key="", val=None, prescale_factor=2048):
    if key not in CONFIG_REGISTERS:
        raise RuntimeError(f"Invalid Config Register Key given: {key}")
    if val is None:
        raise RuntimeError("No Val given to write to Config Register!")

    if CONFIG_REGISTERS[key] == 10:
        if prescale_factor not in VALID_PRESCALE_FACTORS:
            raise RuntimeError("You did not choose a valid prescale factor")
        prescale_bitmask = VALID_PRESCALE_FACTORS[prescale_factor]
        mod_val = ((prescale_bitmask & 0b11) << 12) + (val & 0xfff)
        write_config_reg(ss, 10, mod_val)
    else:
        write_config_reg(ss, CONFIG_REGISTERS[key], val)