"""
SPDX-FileCopyrightText: 2026 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2

Serial interface for a Keithley Series 2400 SourceMeter as voltage source.

Keithley needs to be set to RS-232 mode with:
- BAUD: 19200
- BITS: 8
- PARITY: EVEN
- TERMINATOR: <CR+LF>
- FLOW_CONTROL: NONE
"""

import threading

import pyvisa.constants

from .KeithleyInterface import KeithleyInterface


class Keithley6487(KeithleyInterface):
    def __init__(
        self,
        port: str,
    ) -> None:
        super().__init__(
            port=port,
            baud=19200,
            bits=8,
            stopbits=pyvisa.constants.StopBits.one,
            parity=pyvisa.constants.Parity.even,
            terminator="\r\n",
            flow_control=False,
        )
        self._output_lock = threading.Lock()

    # Device functions

    def reset(self) -> None:
        self._write("*RST")

    def identify(self) -> str:
        return self._query("*IDN?")

    def enable_output(self, enable: bool) -> None:
        with self._output_lock:
            on_off = "ON" if enable else "OFF"
            self._write(f":SOUR:VOLT:STAT {on_off}")

    def output_enabled(self) -> bool:
        ret = self._query(":SOUR:VOLT:STAT?")
        return ret != "0"

    def get_terminals(self) -> list[str]:
        return ["rear"]

    def set_terminal(self, terminal: str) -> None:
        if terminal.lower() not in ["rear"]:
            raise ValueError("Only rear terminal supported")

    def get_terminal(self) -> str:
        terminal = "rear"
        return terminal

    def set_voltage(self, voltage: float) -> None:
        self._write(f":SOUR:VOLT:LEV {voltage}")

    def get_voltage(self) -> float:
        return float(self._query(":SOUR:VOLT:LEV?"))

    def set_ovp(self, voltage: float):  # use voltage range kind of like an ovp?
        self._write(f":SOUR:VOLT:RANG {voltage}")

    def get_ovp(self) -> float:
        return float(self._query(":SOUR:VOLT:RANG?"))

    def set_compliance(self, current: float) -> None:
        self._write(f":SOUR:VOLT:ILIM {current}")

    def get_compliance(self) -> float:
        return float(self._query(":SOUR:VOLT:ILIM?"))

    def in_compliance(self) -> bool:
        with self._output_lock:
            if self.output_enabled():
                status = int(self._query(":STAT:MEAS:COND?"))
                tripped = bool(status & (1 << 14))
                return tripped
        return False

    def read_output(self) -> tuple[float, float, float]:
        with self._output_lock:
            if self.output_enabled():
                voltage, current, timestamp = self._query(":READ?").split(",")
                return float(voltage), float(current), float(timestamp)
        return 0.0, 0.0, 0.0

    # Device helper functions

    def initialize(self) -> None:
        # Set data format to ascii (comma-separated)
        self._write(":FORM:DATA ASC")
        # Output voltage, current and timestamp
        self._write(":FORM:ELEM VSO, READ, TIME")
        # Set buffer to one reading
        self._write(":TRAC:POIN 1")  # codespell:ignore poin
        # Set trigger to take one reading
        self._write(":TRIG:ARM:COUN 1")

    def release(self) -> None:
        self._write(":SYST:LOC")
