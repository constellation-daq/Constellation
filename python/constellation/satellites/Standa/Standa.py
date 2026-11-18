"""
SPDX-FileCopyrightText: 2026 A. Pettersson, DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2

Provides the satellite implementation for a Standa motor controller setup
Constellation interface
"""

from typing import Any

from constellation.core.commandmanager import cscp_requestable
from constellation.core.configuration import Configuration
from constellation.core.protocol.cscp1 import SatelliteState
from constellation.core.satellite import Satellite

from .motor_controller import MODEL_STEPS, Motor


class Standa(Satellite):
    def __init__(self, *args: Any, **kwargs: Any) -> None:
        super().__init__(*args, **kwargs)
        self._motors: dict[str, Motor] = {}
        self._initpos: dict[str, float] = {}

    @property
    def motors(self) -> dict[str, Motor]:
        if not self._motors:
            raise RuntimeError("Stage is not connected")
        return self._motors

    def do_initializing(self, config: Configuration) -> str | None:
        self._initpos.clear()
        self._reset_motors()

        axes_conf = config.get_section("axes")
        for axis in axes_conf.get_keys():
            axis_conf = axes_conf.get_section(axis)

            model = axis_conf.get_str("model")
            if model not in MODEL_STEPS:
                raise ValueError(f"Device model {model} not supported")

            serial = axis_conf.get_int("serial")
            self._motors[axis] = Motor(axis, model, serial, logger=self.log)

            self._initpos[axis] = axis_conf.get_float("position")

        self.calibrate_on_launch = config.get_bool("calibrate_on_launch", False)

        self.reset_metrics()
        # Register scheduled metrics of positions
        for axis, motor in self._motors.items():
            unit = motor.unit
            self.register_scheduled_metric(
                f"POS_{axis.upper()}",
                unit,
                f"Current position of axis {axis}",
                5,
                motor.get_position,
                [SatelliteState.INIT, SatelliteState.ORBIT],
            )

        return f"Connected to {len(axes_conf)} axes!"

    def _move(self, positions: dict[str, float]) -> str:
        for axis, position in positions.items():
            if axis not in self.motors:
                raise ValueError("Given axis is not a registered stage")
            self.motors[axis].move_abs(position)

        for axis, motor in ((a, self.motors[a]) for a in positions):
            if not motor.wait_until_idle():
                raise RuntimeError(f"Axis {axis} did not reach {positions[axis]}{motor.unit} within the timeout")
            self.log.info(f"Moved axis {axis} to {positions[axis]}{motor.unit}")

        return "Moved stages"

    def _reset_motors(self):
        if self._motors:
            for motor in self._motors.values():
                motor.close()
            self._motors.clear()

    def do_starting(self, run_identifier: str) -> str:
        self.log.info(f"Started run {run_identifier}")
        return "Started run"

    def do_launching(self) -> str | None:
        """Optionally calibrate and move stages to config position"""
        if self.calibrate_on_launch:
            for motor in self.motors.values():
                motor.calibrate()
        # Move motors to specified config position
        self._move(self._initpos)

        return "Launched!"

    def do_reconfigure(self, config: Configuration) -> str | None:
        axes_conf = config.get_section("axes")
        position = {axis: axes_conf.get_section(axis).get_float("position") for axis in axes_conf.get_keys()}
        return self._move(position)

    def reentry(self) -> None:
        self._reset_motors()
        super().reentry()

    @cscp_requestable([SatelliteState.INIT])
    def home(self) -> tuple[str, Any, dict[str, Any]]:
        """Move all axes to their low limit switch and define zero there, leaving the stage at zero"""
        position = {}
        for axis, motor in self.motors.items():
            motor.calibrate()
            position[axis] = motor.get_position()

        return f"Homed, stage at {position}", position, {}

    @cscp_requestable([SatelliteState.INIT, SatelliteState.ORBIT])
    def get_position(self, axis: str) -> tuple[str, float | None, dict[str, int]]:
        "Returns the current position of given axis"
        if axis not in self.motors:
            raise ValueError(f"Axis '{axis}' is not a registered stage")
        pos = self.motors[axis].get_position()
        return f"Axis {axis} is at {pos}", pos, {}
