"""
SPDX-FileCopyrightText: 2026 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2

Class to control lasers from Particulars through USB connection.
Adapted from the equivalent class in PyticularsTCT by Matias Senger
"""

import platform
import time

import usb.core

from constellation.core.configuration import Configuration
from constellation.core.satellite import Satellite


def int_to_char_cpp_style(i: int):
    if not isinstance(i, int):
        raise TypeError("`i` must be an int.")
    return int(f"{i:b}"[-8:], 2)


class ParticularsLaserController(Satellite):
    def do_initializing(self, config: Configuration):
        device = usb.core.find(idVendor=0xC251, idProduct=0x2201)  # ID c251:2201 Keil Software, Inc. LASER Driver IJS
        if device is None:
            raise RuntimeError(
                "Cannot find the laser controller within the USB devices. "
                "Be sure it is connected and that it is recognized by the computer."
            )
        if platform.system() == "Linux":
            interface = device[0].interfaces()[0]
            if device.is_kernel_driver_active(
                interface.bInterfaceNumber
            ):  # Not sure why this is needed, but fails without it.
                device.detach_kernel_driver(interface.bInterfaceNumber)

        self.device = device
        self.endpoint = device[0].interfaces()[0].endpoints()[0]

        self.frequency = config.get_float("frequency", min_val=50.0, max_val=100e3)
        self.dac = config.get_int("dac", min_val=0, max_val=1023)

        current_status = self.status
        self._turn_on()  # This updates the real frequency and DAC.
        if current_status == "off":
            self._turn_off()

    def do_launching(self):
        self._turn_on()
        self.log.info(f"Laser at {self.frequency}Hz, {self.DAC} DAC.")

    def do_landing(self):
        self._turn_off()
        self.log.info("Laser is turned off.")
        # return "Laser is turned off."

    def do_interrupting(self):
        self._turn_off()
        self.log.info("Laser is turned off.")

    def do_reconfigure(self, partial_config: Configuration) -> str:
        config_keys = partial_config.get_keys()

        if "dac" in config_keys:
            self.dac = partial_config.get_int("dac", min_val=0)
            self._set_DAC(DAC=self.dac)

        if "frequency" in config_keys:
            self.frequency = partial_config.get_float("frequency", min_val=50.0)
            self._set_frequency(Hz=self.frequency)

        self.log.info(f"Laser reconfigured at {self.frequency} Hz, {self.DAC} DAC.")

    def _send_packet(self, packet):
        """Send a packet of bytes from the computer to the laser controller.
                Sending function parameters established empirically, do not change!
        Parameters
        ----------
        packet: array of int between 0 and 255
                An array of bytes, i.e. an array of integer numbers each
                in 0, 1, ..., 255, to be sent to the laser controller.
                Example `packet = [0,4,200]`.
        """
        if (
            not hasattr(packet, "__iter__")
            or not all([isinstance(byte, int) for byte in packet])
            or any([not 0 <= byte <= 2**8 for byte in packet])
        ):
            raise ValueError(f"`packet` must be an array of integers each between 0 and 255. Received {repr(packet)}.")
        packet = packet + (64 - len(packet)) * [0]
        number_of_bytes_sent = self.device.ctrl_transfer(
            bmRequestType=0x21,
            bRequest=9,
            wValue=0x200,
            wIndex=0,
            data_or_wLength=packet,
        )
        if number_of_bytes_sent != len(packet):
            self.log.warning(f"I tried to send {len(packet)} bytes but only {number_of_bytes_sent} were sent...")
        time.sleep(0.01)  # A small sleep to avoid issues if many commands are sent one after the other.
        return number_of_bytes_sent

    @property
    def status(self):
        """Returns either 'on' or 'off'."""
        for _k in [1, 2]:  # Read twice because otherwise it tends to fail in the first one, don't ask me why...
            data = self.endpoint.read(
                size_or_buffer=64
            )  # This number comes from the C++ code. It is also the `wMaxPacketSize`.
        STATUS_BIT = 6  # This I took from the C++ code.
        if data[STATUS_BIT] == 1:
            return "on"
        elif data[STATUS_BIT] == 0:
            return "off"
        else:
            raise RuntimeError("Cannot understand response from laser controller, dont know if the laser is on or off.")

    @status.setter
    def status(self, status: str):
        """Set the laser 'on' or 'off'."""
        if status not in {"on", "off"}:
            raise ValueError(f'`status` must be either "on" or "off", received {repr(status)}.')
        if status == "on":
            self.on()
        else:
            self.off()

    def on(self):
        """Turn the laser on."""
        if self.status == "off":
            self._turn_on()

    def off(self):
        """Turn the laser off."""
        if self.status == "on":
            self._turn_off()

    def _turn_on(self):
        """Turn on the laser at the configured frequency and DAC values."""
        self._set_frequency(Hz=self.frequency)
        self._set_DAC(DAC=self.dac)
        self._hardware_sequence_enable()

    def _turn_off(self):
        """Turn the laser off."""
        self._send_packet([90])
        self._send_packet([4])

    def _set_frequency(self, Hz: float):
        """Set the frequency of the laser."""
        if not 50 <= Hz <= 100e3:
            raise ValueError(f"`Hz` must be within 50 and 100e3, received {Hz} Hz.")
        Hz = int((500000000 / Hz - 440) / 180)  # Taken from the C++ code.
        cmd = [99, int_to_char_cpp_style(Hz), int_to_char_cpp_style(Hz >> 8)]  # Taken from the C++ code.
        self._send_packet(cmd)

    def _set_DAC(self, DAC: int):
        if not isinstance(DAC, int):
            raise TypeError(f"`DAC` must be an integer number, received {repr(DAC)}.")
        if not 0 <= DAC < 1024:
            raise ValueError("`DAC` must be in [0,1024).")
        self._turn_off()
        self._enable_DAC()
        self._send_packet([94, int_to_char_cpp_style(DAC), int_to_char_cpp_style(DAC >> 8)])  # Taken from the C++ code.
        self._hardware_sequence_enable()

    def _hardware_sequence_enable(self):
        """Turn the laser on?"""
        self._send_packet([91])  # Taken from the C++ code.

    def _enable_DAC(self):
        self._send_packet([92])  # Taken from the C++ code.

    def _disable_DAC(self):
        self._send_packet([93])  # Taken from the C++ code.
