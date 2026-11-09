"""
SPDX-FileCopyrightText: 2026 A. Pettersson, DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2

Provides the satellite implementation for a Standa motor controller setup
Constellation interface
"""

from typing import Any

from constellation.core.commandmanager import cscp_requestable
from constellation.core.configuration import Configuration
from constellation.core.monitoring import schedule_metric
from constellation.core.protocol.cscp1 import SatelliteState
from constellation.core.satellite import Satellite

from .motor_controller import Motors

_SUPPORTED_DEVICES = ["8MT30_50"]


class Standa(Satellite):
    def __init__(self, *args: Any, **kwargs: Any) -> None:

        super().__init__(*args, **kwargs)
        self._motors: Motors | None = None
        self._config: Configuration
        self._serials: dict[str, int] = {}
        self._models: dict[str, str] = {}
        self._initpos: dict[str, float] = {}

    @property
    def motors(self) -> Motors:
        if self._motors is None:
            raise RuntimeError("Stage is not connected")
        return self._motors

    def do_initializing(self, config: Configuration) -> str | None:
        self.log.info("Received configuration with parameters: %s", ", ".join(config.get_keys()))
        self._config = config

        axes_conf = config.get_section("axes")
        for axis in axes_conf.get_keys():
            axis_conf = axes_conf.get_section(axis)

            self._models[axis] = axis_conf.get_str("model")
            if self._models[axis] not in _SUPPORTED_DEVICES:
                raise ValueError(f"Device model {self._models[axis]} not supported")

            self._serials[axis] = axis_conf.get_int("serial")
            self._initpos[axis] = axis_conf.get_float("position")

        if self._motors is None:
            self._motors = Motors(self._serials, logger=self.log)

        self.calibrate_on_launch = config.get_bool("calibrate_on_launch", False)

        return f"Connected to axes {', '.join(self.motors.axes)} with Standa models {', '.join(self._models)}"

    def _move(self, config: Configuration) -> str:
        if "axes" not in config:
            return "Reconfigured without axes"

        axes_conf = config.get_section("axes")
        position = {axis: axes_conf.get_section(axis).get_float("position") for axis in axes_conf.get_keys()}
        if not position:
            return "Reconfigured without position"

        for axis, mm in position.items():
            self.motors.move_abs(axis, mm)

        if not self.motors.wait_until_idle():
            raise RuntimeError(f"Stage did not reach {position} within the timeout")

        self.log.info(f"Moved to {position}")
        return "Moved stages"

    def do_starting(self, run_identifier: str) -> str:
        self.log.info(f"Started run {run_identifier}")
        return "Started run"

    def do_launching(self) -> str | None:
        if self.calibrate_on_launch:
            self.motors.calibrate()
        else:
            # Move motors to specified config position
            self._move(self._config)
        return "Launched!"

    def do_reconfigure(self, config: Configuration) -> str | None:
        return self._move(config)

    def reentry(self) -> None:
        if self._motors is not None:
            self._motors.close()
        super().reentry()

    def do_stopping(self) -> str:
        self.log.info("Stopping the run")
        return "Stopped run"

    @cscp_requestable([SatelliteState.INIT])
    def home(self) -> tuple[str, Any, dict[str, Any]]:
        """Move all axes to their low limit switch and define zero there, leaving the stage at zero"""
        self.motors.calibrate()
        position = {axis: self.motors.get_position(axis) for axis in self.motors.axes}
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
        motors = self._motors
        if motors is None or axis not in motors.axes:
            return None
        return motors.get_position(axis)

    @cscp_requestable([SatelliteState.INIT, SatelliteState.ORBIT])
    def position_x(self) -> tuple[str, float | None, dict[str, int]]:
        "Returns the current x position of the stage"
        pos_x = self._position("x")
        return f"Stage x is at {pos_x}", pos_x, {}

    @cscp_requestable([SatelliteState.INIT, SatelliteState.ORBIT])
    def position_y(self) -> tuple[str, float | None, dict[str, int]]:
        "Returns the current y position of the stage"
        pos_y = self._position("y")
        return f"Stage y is at {pos_y}", pos_y, {}

    @cscp_requestable([SatelliteState.INIT, SatelliteState.ORBIT])
    def position_z(self) -> tuple[str, float | None, dict[str, int]]:
        "Returns the current z position of the stage"
        pos_z = self._position("z")
        return f"Stage z is at {pos_z}", pos_z, {}
