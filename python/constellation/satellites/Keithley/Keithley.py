"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2

Provides the class for the Keithley satellite
"""

import time
from typing import Any

from constellation.core.commandmanager import cscp_requestable
from constellation.core.configuration import Configuration
from constellation.core.message.cscp1 import CSCP1Message
from constellation.core.monitoring import schedule_metric
from constellation.core.protocol.cscp1 import SatelliteState, states_except
from constellation.core.satellite import Satellite

from .Keithley2410 import Keithley2410
from .KeithleyInterface import KeithleyInterface

_SUPPORTED_DEVICES = {
    "2410": Keithley2410,
}


class Keithley(Satellite):
    device: KeithleyInterface

    def do_initializing(self, config: Configuration) -> None:
        device_name = config.get_str("device")
        if device_name not in _SUPPORTED_DEVICES.keys():
            raise ValueError(f"Device {device_name} not supported")

        self.device = _SUPPORTED_DEVICES[device_name](config.get_str("port"))

        self.voltage = config.get_float("voltage")
        self.voltage_step = config.get_float("voltage_step", min_val=0.0)
        self.settle_time = config.get_float("settle_time", min_val=0.0)

        self.ovp = config.get_float("ovp", min_val=0.0)
        self.compliance = config.get_float("compliance", min_val=0.0)

        # If one terminal, select as default, otherwise get from config
        terminals = self.device.get_terminals()
        if len(terminals) == 1:
            self.terminal = terminals[0]
        else:
            self.terminal = config["terminal"]
            if self.terminal not in terminals:
                raise ValueError(f"{self.terminal} not a valid terminal (choose from {terminals})")

        self.log.info(f"Initializing Keithley {device_name}")
        self.device.initialize()
        identify = self.device.identify()
        if not identify:
            raise ConnectionError("No connection to Keithley")
        self.log.info("Device: %s", identify)

    def do_launching(self) -> str:
        self.device.set_terminal(self.terminal)
        self._set_ovp()
        self._set_compliance()

        # Set voltage to zero in case it wasn't properly reset for some reason
        self.device.enable_output(False)
        self.device.set_voltage(0.0)

        # Ramp to target voltage
        self.device.enable_output(True)
        self._ramp(self.voltage)

        return f"Keithley at {self.device.get_voltage()}V"

    def do_landing(self) -> str:
        # Ramp to zero, then disable output
        self._ramp(0.0)
        self.device.enable_output(False)

        return f"Keithley at {self.device.get_voltage()}V (output disabled)"

    def do_reconfigure(self, partial_config: Configuration) -> str:
        config_keys = partial_config.get_keys()

        if "device" in config_keys:
            raise ValueError("Reconfiguring device is not possible")
        if "port" in config_keys:
            raise ValueError("Reconfiguring port is not possible")
        if "terminal" in config_keys:
            raise ValueError("Reconfiguring terminal is not possible")

        if "ovp" in config_keys:
            self.ovp = partial_config.get_float("ovp", min_val=0.0)
            self._set_ovp()
        if "compliance" in config_keys:
            self.compliance = partial_config.get_float("compliance", min_val=0.0)
            self._set_compliance()

        if "voltage_step" in config_keys:
            self.voltage_step = partial_config.get_float("voltage_step", min_val=0.0)
        if "settle_time" in config_keys:
            self.voltage_step = partial_config.get_float("settle_time", min_val=0.0)

        # If voltage changed, ramp to new voltage
        if "voltage" in config_keys:
            self.voltage = partial_config.get_float("voltage")
            self._ramp(self.voltage)

        return f"Keithley at {self.device.get_voltage()}V"

    def fail_gracefully(self) -> None:
        # Try to ramp down
        self.log.info("Attempting to ramp down after failure")
        try:
            self._ramp(0.0)
            self.device.enable_output(False)
        except Exception:
            self.log.warning("Failed to ramp down")

    def reentry(self) -> None:
        if hasattr(self, "device"):
            self.device.release()
        super().reentry()

    def _set_ovp(self):
        self.log.info(f"Setting OVP to {self.ovp}V")
        self.device.set_ovp(self.ovp)
        device_ovp = self.device.get_ovp()
        if device_ovp != self.ovp:
            raise ValueError(f"OVP set to {self.ovp}V but {device_ovp}V was applied (check manual for supported values)")

    def _set_compliance(self):
        self.log.info(f"Setting compliance to {self.compliance}A")
        self.device.set_compliance(self.compliance)
        device_compliance = self.device.get_compliance()
        if device_compliance != self.compliance:
            raise ValueError(f"Compliance set to {self.compliance}A but {device_compliance}A was applied")

    def _ramp(self, voltage_target: float):
        voltage_current = self.device.get_voltage()
        ramp_up = voltage_target > voltage_current

        self.log.info(f"Ramping output voltage from {voltage_current}V {'up' if ramp_up else 'down'} to {voltage_target}V")

        # Lambda to evaluate if another step should be added
        voltage_step = self.voltage_step
        do_next_step = lambda voltage: voltage + voltage_step < voltage_target  # noqa: E731
        if not ramp_up:
            voltage_step *= -1
            do_next_step = lambda voltage: voltage + voltage_step > voltage_target  # noqa: E731

        while voltage_current != voltage_target:
            # Check if another step can be added without exceeding target
            if do_next_step(voltage_current):
                voltage_current += voltage_step
            else:
                voltage_current = voltage_target

            # Set voltage
            self.log.debug(f"Setting output voltage to {voltage_current}V")
            self.device.set_voltage(voltage_current)
            # Send metrics
            voltage, current, timestamp = self.device.read_output()
            in_compliance = self.device.in_compliance()
            self.stat("VOLTAGE", voltage)
            self.stat("CURRENT", current)
            self.stat("IN_COMPLIANCE", in_compliance)
            # Settle
            time.sleep(self.settle_time)

        self.log.info(f"Ramped output voltage to {self.device.get_voltage()}V")

    @cscp_requestable(
        states_except([SatelliteState.NEW, SatelliteState.initializing, SatelliteState.reconfiguring, SatelliteState.ERROR])
    )
    def identify(self, request: CSCP1Message) -> tuple[str, Any, dict[str, Any]]:
        """Identify device (includes serial number)"""
        return self.device.identify(), None, {}

    @cscp_requestable(
        states_except([SatelliteState.NEW, SatelliteState.initializing, SatelliteState.reconfiguring, SatelliteState.ERROR])
    )
    def in_compliance(self, request: CSCP1Message) -> tuple[str, Any, dict[str, Any]]:
        """Check if current is in compliance"""
        in_compliance = self.device.in_compliance()
        is_str = "is" if in_compliance else "is not"
        return f"Device {is_str} in compliance", in_compliance, {}

    @cscp_requestable(
        states_except([SatelliteState.NEW, SatelliteState.initializing, SatelliteState.reconfiguring, SatelliteState.ERROR])
    )
    def read_output(self, request: CSCP1Message) -> tuple[str, Any, dict[str, Any]]:
        """Read voltage and current output"""
        voltage, current, timestamp = self.device.read_output()
        return f"Output: {voltage}V, {current}A", {"voltage": voltage, "current": current, "timestamp": timestamp}, {}

    @schedule_metric("V", 5)
    def VOLTAGE(self) -> float | None:
        return self.device.read_output()[0]

    @schedule_metric("A", 5)
    def CURRENT(self) -> float | None:
        return self.device.read_output()[1]

    @schedule_metric("", 5)
    def IN_COMPLIANCE(self) -> float | None:
        return self.device.in_compliance()
