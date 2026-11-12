"""
SPDX-FileCopyrightText: 2026 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2

Functionality for interacting with motor controllers.
Original code by Daniil D.Rastorguev.
"""

import glob
import logging
import time
from typing import Any

import libximc.highlevel as ximc


def steps_to_mm_8MT30_50(steps, usteps):
    return (steps + float(usteps) / 256.0) * 1.25 / 1000


def mm_to_steps_8MT30_50(mm):
    mm = int(mm / 1.25 * 1000 * 256)
    return mm // 256, mm % 256


MODEL_STEPS = {"8MT30_50": (steps_to_mm_8MT30_50, mm_to_steps_8MT30_50)}


class Motor:
    """
    Connection to one ximc controller.
    """

    def __init__(self, axis_id: str, model: str, serial: Any, logger=None):
        self.log = logger if logger is not None else logging.getLogger(__name__)
        self._axis_id = axis_id
        self._model = model
        self._serial = f"{serial:08X}" if isinstance(serial, int) else str(serial).strip().upper()
        self._axis: ximc.Axis | None = None

        if model not in MODEL_STEPS:
            raise ValueError(f"Step conversion missing for model {model}")
        self.steps_to_mm = MODEL_STEPS[model][0]
        self.mm_to_steps = MODEL_STEPS[model][1]

        # The by-id name contains the serial in hex
        port = next((p for p in glob.glob("/dev/serial/by-id/usb-XIMC*") if self._serial in p.upper()), None)
        if not port:
            raise RuntimeError(f"Missing controller for axis {axis_id} (S/N {self._serial})")

        axis = ximc.Axis("xi-com://" + port)  # Linux implementation
        axis.open_device()
        self._axis = axis
        self.log.info(f"Connected {port}, S/N {self._serial}")

    def __del__(self):
        self.close()

    @property
    def axis(self) -> ximc.Axis:
        if self._axis is None:
            raise RuntimeError(f"Axis '{self._axis_id}' is not connected")
        return self._axis

    def close(self):
        if self._axis is not None:
            self._axis.close_device()
            self._axis = None

    def calibrate(self):
        self.log.info(f"Calibrating axis {self._axis_id}...")
        self.axis.command_home()
        self.axis.command_wait_for_stop(10)

        time.sleep(1)

        self.axis.command_zero()
        self.log.info("Motor calibration finished")

    def get_position(self):
        pos = self.axis.get_position()
        steps, usteps = pos.Position, pos.uPosition
        return self.steps_to_mm(steps, usteps)

    def is_moving(self):
        status = self.axis.get_status()
        return bool(status.MvCmdSts & ximc.MvcmdStatus.MVCMD_RUNNING)

    def move_abs(self, mm):
        if not 0.0 <= mm <= 50.0:
            raise ValueError(f"Target {mm} mm of axis '{self._axis_id}' is outside the travel range 0mm - 50mm")
        steps, usteps = self.mm_to_steps(mm)
        self.axis.command_move(steps, usteps)

    def move_rel(self, mm):
        destination = mm + self.get_position()
        self.move_abs(destination)

    def wait_for_stop(self):
        self.axis.command_wait_for_stop(50)

    def wait_until_idle(self, timeout=30.0, poll_interval=0.05):
        time.sleep(poll_interval)
        deadline = time.monotonic() + timeout
        while self.is_moving():
            if time.monotonic() > deadline:
                return False
            time.sleep(poll_interval)
        return True
