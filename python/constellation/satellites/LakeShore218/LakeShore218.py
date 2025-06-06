"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2

This module provides the class for the LakeShore218 satellite.
"""

from functools import partial
from threading import Lock
from typing import Any

import pyvisa
import pyvisa.constants

from constellation.core.cmdp import MetricsType
from constellation.core.commandmanager import cscp_requestable
from constellation.core.configuration import Configuration
from constellation.core.message.cscp1 import CSCP1Message
from constellation.core.satellite import Satellite


class LakeShore218(Satellite):
    _serial: pyvisa.resources.SerialInstrument

    def __init__(self, *args, **kwargs):
        self._rm = pyvisa.ResourceManager()
        self._lock = Lock()
        super().__init__(*args, **kwargs)

    def do_initializing(self, config: Configuration) -> None:
        # Get serial config
        port = config["port"]
        visa_address = f"ASRL{port}::INSTR"

        # Get metrics config
        channel_names = config.setdefault("channel_names", [f"TEMP_{n + 1}" for n in range(8)])
        sampling_interval = config.setdefault("sampling_interval", 5)

        # Check that channel names are unique and eight in total
        if len(set(channel_names)) != 8:
            raise Exception("`channel_names` parameter requires eight unique names")

        # Open VISA device
        self.log.debug("Opening VISA device %s", visa_address)
        self._serial = self._rm.open_resource(  # type: ignore
            visa_address,
            baud_rate=9600,
            data_bits=7,
            stop_bits=pyvisa.constants.StopBits.one,
            parity=pyvisa.constants.Parity.odd,
            read_termination="\r\n",
            write_termination="\r\n",
        )

        # Register metrics
        self.reset_scheduled_metrics()
        for n in range(8):
            self.schedule_metric(
                channel_names[n], "K", MetricsType.LAST_VALUE, sampling_interval, partial(self._get_temp, n + 1)
            )

    def _get_temp(self, channel: int) -> float | None:
        # Check that _serial exists (required for metrics before INIT)
        if not hasattr(self, "_serial"):
            return None
        # Lock required since serial not thread safe
        with self._lock:
            # Check if input is enabled (return "0" or "1")
            if self._serial.query(f"INPUT? {channel}") == "1":
                return float(self._serial.query(f"KRDG? {channel}"))
        # Return None if not enabled
        return None

    @cscp_requestable
    def get_temp(self, request: CSCP1Message) -> tuple[str, Any, dict[str, Any]]:
        channel = 0
        try:
            channel = int(request.payload[0])
        except (TypeError, ValueError):
            raise Exception("Requires channel number as parameter")
        if channel < 1 or channel > 8:
            raise Exception(f"Channel {channel} does not exist")
        temp = self._get_temp(channel)
        verb = f"{temp}K" if temp else "Disabled"
        return verb, temp, {}
