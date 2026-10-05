"""
SPDX-FileCopyrightText: 2026 A. Pettersson, DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2

Provides the satellite implementation for a Standa motor controller setup
Constellation interface
"""

import time
from typing import Any

from .stage_config import Standa_8MT30_50
from .motor_controller import Motors

from constellation.core.commandmanager import cscp_requestable
from constellation.core.configuration import Configuration
from constellation.core.monitoring import schedule_metric
from constellation.core.protocol.cscp1 import SatelliteState
from constellation.core.satellite import Satellite

_SUPPORTED_DEVICES = {
    "8MT30_50": Standa_8MT30_50,
}

class Standa(Satellite):
    def __init__(self, *args: Any, **kwargs: Any) -> None:

        super().__init__(*args, **kwargs)
        self.start_time = time.time()
        self.motors = None

    def do_initializing(self, config: Configuration) -> str | None:

        device_model = config.get_str("model")
        if device_model not in _SUPPORTED_DEVICES.keys():
            raise ValueError(f"Device model {device_model} not supported")

        self.profile = _SUPPORTED_DEVICES[device_model]

        # Serial number of the controller of every axis
        serials_section = config.get_section("axis_serials")
        serials = {axis: serials_section.get_int(axis) for axis in serials_section.get_keys()}

        if not self.motors:
            self.motors = Motors(serials, profile=self.profile, logger=self.log)

        self.calibrate_on_launch = config.get_bool("calibrate_on_launch", False)

        position_section = config.get_section("position")
        self.position = {axis: position_section.get_float(axis) for axis in position_section.get_keys()}

        return f"Connected to axes {', '.join(self.motors.ids)} of a Standa {device_model} stage"

    def do_starting(self, run_identifier: str) -> str:
        self.start_time = time.time()
        return f"Started run {run_identifier}"

    def do_launching(self) -> str | None:
        # Calibrate to zero
        if self.calibrate_on_launch:
            self.motors.calibrate()
            
        return "Launched!"

    def do_reconfigure(self, config: Configuration) -> str | None:
        if "position" not in config:
            return "Reconfigured without position"
        
        position_section = config.get_section("position")
        position = {axis: position_section.get_float(axis) for axis in position_section.get_keys()}

        for axis, mm in position.items():
            self.motors.move_abs(axis, mm)

        if not self.motors.wait_until_idle():
            raise RuntimeError(f"Stage did not reach {position} within the timeout")

        return f"Moved to {position}"

    def reentry(self) -> None:
        if hasattr(self, "motors"):
            self.motors.close()
        super().reentry()

    def do_stopping(self) -> str:
        run_duration = time.time() - self.start_time
        return f"Stopped after {run_duration:.1f}s"

    @cscp_requestable([SatelliteState.INIT, SatelliteState.ORBIT])
    def home(self) -> tuple[str, Any, dict[str, Any]]:
        """Move all axes to their low limit switch and define zero there, leaving the stage at zero"""
        self.motors.calibrate()
        position = {axis: self.motors.get_position(axis) for axis in self.motors.ids}
        return f"Homed, stage at {position}", position, {}

    @schedule_metric("mm", 5)
    def POS_X(self) -> float | None:
        """Current position of the x axis"""
        return self._position("x")

    @schedule_metric("mm", 5)
    def POS_Y(self) -> float | None:
        """Current position of the y axis"""
        return self._position("y")

    @schedule_metric("mm", 5)
    def POS_Z(self) -> float | None:
        """Current position of the z axis"""
        return self._position("z")

    def _position(self, axis: str) -> float | None:
        """Position of an axis, or None while the stage is not connected"""
        motors = getattr(self, "motors", None)
        if motors is None or axis not in motors.ids:
            return None
        return motors.get_position(axis)
    
    @cscp_requestable([SatelliteState.INIT, SatelliteState.ORBIT])
    def position_x(self) -> float | None:
        "Returns the current x position of the stage"
        pos_x = self._position("x")
        return f"Stage x is at {pos_x}", pos_x, {}

    @cscp_requestable([SatelliteState.INIT, SatelliteState.ORBIT])
    def position_y(self) -> float | None:
        "Returns the current y position of the stage"
        pos_y = self._position("y")
        return f"Stage y is at {pos_y}", pos_y, {}

    @cscp_requestable([SatelliteState.INIT, SatelliteState.ORBIT])
    def position_z(self) -> float | None:
        "Returns the current z position of the stage"
        pos_z = self._position("z")
        return f"Stage z is at {pos_z}", pos_z, {}
