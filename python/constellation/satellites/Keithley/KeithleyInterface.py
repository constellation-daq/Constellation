"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2

Abstract serial interface for Keithleys.
"""

from abc import ABCMeta, abstractmethod
from threading import Lock

import pyvisa
import pyvisa.constants


class KeithleyInterface(metaclass=ABCMeta):
    def __init__(
        self,
        port: str,
        baud: int,
        bits: int,
        stopbits: pyvisa.constants.StopBits,
        parity: pyvisa.constants.Parity,
        terminator: str,
        flow_control: bool,
        timeout: float = 1000,
    ) -> None:
        self._rm = pyvisa.ResourceManager()
        visa_address = f"ASRL{port}::INSTR"
        self._serial: pyvisa.resources.SerialInstrument = self._rm.open_resource(  # type: ignore
            visa_address,
            baud_rate=baud,
            data_bits=bits,
            stop_bits=stopbits,
            parity=parity,
            flow_control=flow_control,
            read_termination=terminator,
            write_termination=terminator,
            timeout=timeout,
        )
        self._lock = Lock()

    # Serial helper functions

    def _write(self, command: str) -> None:
        """
        Write command to serial port
        """
        with self._lock:
            self._serial.write(command)

    def _query(self, command: str) -> str:
        """
        Write command to serial and then read until terminator or timeout
        """
        with self._lock:
            return self._serial.query(command)

    # Device functions

    @abstractmethod
    def reset(self) -> None:
        """
        Resets device settings
        """
        ...

    @abstractmethod
    def identify(self) -> str:
        """
        Identifies device
        """
        ...

    @abstractmethod
    def enable_output(self, enable: bool) -> None:
        """
        Enable or disable output
        """
        ...

    @abstractmethod
    def output_enabled(self) -> bool:
        """
        If the output is enabled
        """
        ...

    @abstractmethod
    def get_terminals(self) -> list[str]:
        """
        List of terminals which can be controlled
        """
        ...

    @abstractmethod
    def set_terminal(self, terminal: str) -> None:
        """
        Terminal for which to control output
        """
        ...

    @abstractmethod
    def get_terminal(self) -> str:
        """
        Get terminal for which output is controlled
        """
        ...

    @abstractmethod
    def set_voltage(self, voltage: float) -> None:
        """
        Set output voltage
        """
        ...

    @abstractmethod
    def get_voltage(self) -> float:
        """
        Get output voltage
        """
        ...

    @abstractmethod
    def set_ovp(self, voltage: float) -> None:
        """
        Set over-voltage protection voltage
        """
        ...

    @abstractmethod
    def get_ovp(self) -> float:
        """
        Get over-voltage protection voltage
        """
        ...

    @abstractmethod
    def set_compliance(self, current: float) -> None:
        """
        Set current compliance
        """
        ...

    @abstractmethod
    def get_compliance(self) -> float:
        """
        Get current compliance
        """
        ...

    @abstractmethod
    def in_compliance(self) -> bool:
        """
        If current is in compliance
        """
        ...

    @abstractmethod
    def read_output(self) -> tuple[float, float, float]:
        """
        Reads voltage, current, timestamp
        """
        ...

    # Device helper functions

    @abstractmethod
    def initialize(self) -> None:
        """
        Resets device and initializes appropriate settings
        """
        ...

    @abstractmethod
    def release(self) -> None:
        """
        Release device from remote mode
        """
        ...
