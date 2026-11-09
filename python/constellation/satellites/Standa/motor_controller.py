"""
SPDX-FileCopyrightText: 2026 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2

Functionality for interacting with motor controllers.
Original code by Daniil D.Rastorguev.
"""

import glob
import logging
import time

import libximc.highlevel as ximc


def steps_to_mm(steps, usteps):
    return (steps + float(usteps) / 256.0) * 1.25 / 1000


def mm_to_steps(mm):
    mm = int(mm / 1.25 * 1000 * 256)
    return mm // 256, mm % 256


class Motors:
    """
    Connection to the ximc controllers of a multi-axis stage.
    """

    def __init__(self, serials, logger=None):
        self.log = logger if logger is not None else logging.getLogger(__name__)
        self.axes = {}
        sn_to_axis = {str(serial): axis for axis, serial in serials.items()}

        for port in sorted(glob.glob("/dev/ttyACM*")):
            axis = ximc.Axis("xi-com://" + port)  # Linux impl
            try:
                axis.open_device()
                serial = str(axis.get_serial_number())
            except Exception as e:
                self.log.warning(f"Could not open {port}: {e}")
                continue

            if serial in sn_to_axis:
                self.axes[sn_to_axis[serial]] = axis
                self.log.info(f"Connected {port}, S/N {serial}")
            else:
                axis.close_device()

        missing = set(serials) - set(self.axes)
        if missing:
            raise Exception(f"Missing controllers for axes: {sorted(missing)}")

    def __del__(self):
        self.close()

    def close(self):
        for axis in self.axes.values():
            axis.close_device()
        self.axes.clear()

    def calibrate(self):
        self.log.info("Calibrating motors...")
        for axis in self.axes.values():
            axis.command_home()
        for axis in self.axes.values():
            axis.command_wait_for_stop(10)

        time.sleep(1)

        for axis in self.axes.values():
            axis.command_zero()
        self.log.info("Motor calibration finished")

    def get_position(self, axis):
        pos = self.axes[axis].get_position()
        steps, usteps = pos.Position, pos.uPosition
        return steps_to_mm(steps, usteps)

    def is_moving(self, axis):
        status = self.axes[axis].get_status()
        return bool(status.MvCmdSts & ximc.MvcmdStatus.MVCMD_RUNNING)

    def move_abs(self, axis, mm):
        if not 0.0 <= mm <= 50.0:
            raise ValueError(f"Target {mm} mm of axis '{axis}' is outside the travel range 0mm - 50mm")
        steps, usteps = mm_to_steps(mm)
        self.axes[axis].command_move(steps, usteps)

    def move_rel(self, axis, mm):
        destination = mm + self.get_position(axis)
        self.move_abs(axis, destination)

    def wait_for_stop(self):
        for axis in self.axes.values():
            axis.command_wait_for_stop(50)

    def wait_until_idle(self, timeout=30.0, poll_interval=0.05):
        time.sleep(poll_interval)
        deadline = time.monotonic() + timeout
        while any(self.is_moving(axis) for axis in self.axes):
            if time.monotonic() > deadline:
                return False
            time.sleep(poll_interval)
        return True
