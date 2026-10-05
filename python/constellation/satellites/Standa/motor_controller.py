"""
SPDX-FileCopyrightText: 2026 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2

Functionality for interacting with motor controllers.
Original code by Daniil D.Rastorguev.
"""
import time
import logging

from libximc.lowlevel import *
from libximc.lowlevel import lib as ximc


def steps_to_mm(steps, usteps):
    return (steps + float(usteps) / 256.0) * 1.25 / 1000

def mm_to_steps(mm):
    mm = int(mm / 1.25 * 1000 * 256)
    return mm // 256, mm % 256


class Motors:
    """
    Connection to the ximc controllers of a multi-axis stage.
    """
    def __init__(self, serials, profile=None, logger=None):
        self.ids = {}
        self.log = logger if logger is not None else logging.getLogger(__name__)
        sn_to_axis = {int(sn): axis for axis, sn in serials.items()}


        try:
            device_enum = ximc.enumerate_devices(EnumerateFlags.ENUMERATE_PROBE, '')
            device_count = ximc.get_device_count(device_enum)
            self.log.info(f'ximc device count: {device_count}')
        except Exception:
            self.log.critical('Failed to detect ximc devices')
            raise

        if not device_count:
            self.log.critical('No ximc devices found!')
            raise Exception()

        for device_index in range(device_count):

            try:
                device_name = ximc.get_device_name(device_enum, device_index)
                device_id = ximc.open_device(device_name)
                if profile is not None:
                    profile(ximc, device_id)
            except Exception:
                self.log.critical('Failed to open ximc device')
                self.close()
                raise

            serial_number = c_uint()
            ximc.get_serial_number(device_id, byref(serial_number))
            serial_number = serial_number.value
            if serial_number not in sn_to_axis:
                self.log.warning(f'Ignoring ximc device with unconfigured S/N {serial_number}')
                continue
            self.ids[sn_to_axis[serial_number]] = device_id

            self.log.info(f'Connected {device_name.decode()}, id: {device_id}, S/N: {serial_number}, Axis: {sn_to_axis[serial_number]}')

    def __del__(self):
        self.close()

    def close(self):
        for device_id in self.ids.values():
            ximc.close_device(byref(cast(device_id, POINTER(c_int))))
        self.ids.clear()

    def calibrate(self):
        self.log.info('Calibrating motors...')
        for device_id in self.ids.values():
            ximc.command_home(device_id)
        for device_id in self.ids.values():
            ximc.command_wait_for_stop(device_id, 10)

        time.sleep(1)

        for device_id in self.ids.values():
            ximc.command_zero(device_id)
        self.log.info('Motor calibration finished')

    def get_position(self, axis):
        pos = get_position_t()
        ximc.get_position(self.ids[axis], byref(pos))
        # microsteps ignored
        steps, usteps = pos.Position, pos.uPosition
        return steps_to_mm(steps, usteps)

    def is_moving(self, axis):
        status = status_t()
        ximc.get_status(self.ids[axis], byref(status))

        return bool(status.MvCmdSts & MvcmdStatus.MVCMD_RUNNING)

    def move_abs(self, axis, mm):
        if not 0.0 <= mm <= 50.0:
            raise ValueError(f"Target {mm} mm of axis '{axis}' is outside the travel range 0mm - 50mm")

        steps, usteps = mm_to_steps(mm)
        ximc.command_move(self.ids[axis], steps, usteps)

    def move_rel(self, axis, mm):
        destination = mm + self.get_position(axis)
        self.move_abs(axis, destination)

    def wait_for_stop(self):
        for device_id in self.ids.values():
            ximc.command_wait_for_stop(device_id, 50)

    def wait_until_idle(self, timeout=30.0, poll_interval=0.05):
        time.sleep(poll_interval)
        deadline = time.monotonic() + timeout
        while any(self.is_moving(axis) for axis in self.ids):
            if time.monotonic() > deadline:
                return False
            time.sleep(poll_interval)
        return True
