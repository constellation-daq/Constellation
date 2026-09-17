"""
SPDX-FileCopyrightText: 2026 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2

Async CLI controller with synchronous IPython interface.
"""

import asyncio
import threading
import time
from datetime import datetime
from typing import Any

from constellation.core.async_experimental.async_basecontroller import AsyncScriptableController
from constellation.core.chirp import CHIRPServiceIdentifier
from constellation.core.commandmanager import get_cscp_commands
from constellation.core.controller import ControllerState, SatelliteArray, SatelliteResponse, SatelliteUpdate
from constellation.core.protocol.cscp1 import SatelliteState
from constellation.core.satellite import Satellite
from constellation.core.util import case_insensitive_dict


class AsyncCLIController:
    """IPython-facing controller backed by AsyncScriptableController.

    Runs the async event loop in a daemon thread and provides synchronous
    command methods suitable for interactive use.
    """

    def __init__(
        self,
        group: str,
        name: str = "CLIController",
        interface: list[str] | None = None,
        log_level: str = "INFO",
    ) -> None:
        self._ctrl = AsyncScriptableController(
            group=group,
            name=name,
            interface=interface,
            log_level=log_level,
        )
        self._ctrl._on_satellite_update = self._on_satellite_update

        self.group = group
        self._constellation = SatelliteArray(group, self.command)

        self._loop: asyncio.AbstractEventLoop | None = None
        self._loop_ready = threading.Event()
        self._stop_flag = threading.Event()
        self._thread = threading.Thread(target=self._run_loop, daemon=True, name="async-controller")
        self._thread.start()
        self._loop_ready.wait(timeout=5.0)

        # Allow CHIRP discovery to complete
        time.sleep(2)

    @property
    def log(self) -> Any:
        """Forward log access to the underlying controller."""
        return self._ctrl.log

    def _run_loop(self) -> None:
        """Background thread running the async event loop."""
        self._loop = asyncio.new_event_loop()
        asyncio.set_event_loop(self._loop)
        self._loop_ready.set()
        stop = asyncio.Event()

        async def _watch_stop() -> None:
            while not self._stop_flag.is_set():
                await asyncio.sleep(0.2)
            stop.set()

        async def _main() -> None:
            asyncio.ensure_future(_watch_stop())
            run_task = asyncio.ensure_future(self._ctrl.run(stop))
            await asyncio.sleep(0.3)
            self._ctrl.request(CHIRPServiceIdentifier.CONTROL)
            await asyncio.sleep(0.1)
            self._ctrl.request(CHIRPServiceIdentifier.HEARTBEAT)
            await run_task
            await self._ctrl.shutdown()

        self._loop.run_until_complete(_main())

    def _on_satellite_update(self, canonical_name: str, update_type: SatelliteUpdate) -> None:
        """Update the SatelliteArray when satellites connect or disconnect."""
        sat_type, sat_name = canonical_name.split(".", maxsplit=1)
        if update_type == SatelliteUpdate.ADDED:
            commands = self._ctrl.get_available_cscp_commands(canonical_name)
            if not commands:
                commands = get_cscp_commands(Satellite)
            self._constellation._add_satellite(sat_name, sat_type, commands)
        elif update_type == SatelliteUpdate.REMOVED:
            try:
                from constellation.core.chirp import get_uuid

                uuid = str(get_uuid(canonical_name))
                self._constellation._remove_satellite(uuid)
            except KeyError:
                pass

    @property
    def constellation(self) -> SatelliteArray:
        """Return the SatelliteArray of controlled satellites."""
        return self._constellation

    @property
    def states(self) -> case_insensitive_dict[SatelliteState]:
        """Return current satellite states from heartbeat tracking."""
        return self._ctrl.heartbeat_states

    @property
    def last_state_change(self) -> case_insensitive_dict[datetime]:
        """Return last state change timestamps from heartbeat tracking."""
        return self._ctrl.heartbeat_state_changes

    @property
    def state(self) -> ControllerState:
        """Return the global state of the constellation."""
        if len(self.states) == 0:
            return ControllerState.NEW
        if any(
            state in self.states.values()
            for state in [
                SatelliteState.ERROR,
                SatelliteState.DEAD,
                SatelliteState.SAFE,
            ]
        ):
            return ControllerState.ERROR
        if any(
            state in self.states.values()
            for state in [
                SatelliteState.initializing,
                SatelliteState.launching,
                SatelliteState.landing,
                SatelliteState.reconfiguring,
                SatelliteState.starting,
                SatelliteState.stopping,
                SatelliteState.interrupting,
            ]
        ):
            return ControllerState.TRANSITIONING
        for target in [
            ControllerState.NEW,
            ControllerState.INIT,
            ControllerState.ORBIT,
            ControllerState.RUN,
        ]:
            if any(state.value == target.value for state in self.states.values()):
                return target
        return ControllerState.ERROR

    @property
    def status(self) -> str:
        """Return a human-readable summary of the constellation state."""
        res = []
        for state in SatelliteState:
            sats = [sat for sat, sat_state in self.states.items() if sat_state == state]
            if sats:
                res.append(f"{len(sats)} Satellite{'s are' if len(sats) > 1 else ' is'} in {state.name}")
        prefix = f"{len(self.constellation.satellites)} connected: "
        if len(res) == 1:
            return prefix + "All " + res[0]
        return prefix + ", ".join(res)

    def command(self, cmd: str, payload: Any, sat_type: str | None, sat_name: str | None) -> Any:
        """Send a CSCP command and block until the response arrives."""
        future = asyncio.run_coroutine_threadsafe(
            self._ctrl.command(cmd, sat=sat_name or "", satcls=sat_type or "", payload=payload),
            self._loop,
        )

        sat_response = SatelliteResponse()
        try:
            ret_msg = future.result(timeout=15.0)
        except RuntimeError as e:
            sat_response.success = False
            sat_response.errmsg = repr(e)
            return sat_response
        except TimeoutError:
            sat_response.success = False
            sat_response.errmsg = "Command timed out"
            return sat_response

        if ret_msg:
            sat_response.msg = ret_msg.verb_msg
            sat_response.payload = ret_msg.payload
            sat_response.meta = ret_msg.tags

        return sat_response

    def await_state(self, target: SatelliteState, timeout: int = 60) -> None:
        """Block until the desired global state is reached."""
        start = time.time()

        # Wait for pending heartbeat transitions to settle
        while True:
            outdated = [hb for hb in self._ctrl._hb_receiver._states.values() if hb.outdated]
            if not outdated:
                break
            if time.time() - start > timeout:
                raise RuntimeError(
                    f"Timeout after {timeout}s while waiting for state {target.name}: "
                    f"{[hb.name for hb in outdated]} still have an outdated state"
                )
            time.sleep(0.1)

        remaining = timeout - (time.time() - start)
        while not all(state == target for state in self.states.values()):
            if time.time() - start > remaining:
                raise RuntimeError(f"Timeout after {timeout}s while waiting for state {target.name}")
            if any(state == SatelliteState.ERROR for state in self.states.values()):
                raise RuntimeError(f"ERROR state detected while waiting for state {target.name}")
            time.sleep(0.1)

    def await_satellites(self, satellites: list[str], timeout: int = 60) -> None:
        """Block until all named satellites are connected."""
        start = time.time()
        while not set(satellites).issubset(self._constellation.satellites.keys()):
            if time.time() - start > timeout:
                not_found = ", ".join(set(satellites).difference(self._constellation.satellites.keys()))
                raise RuntimeError(f"Timeout after {timeout}s while waiting for satellites: could not find {not_found}")
            time.sleep(0.1)

    def await_n_satellites(self, satellites: int, timeout: int = 60) -> None:
        """Block until a defined number of satellites is connected."""
        start = time.time()
        while len(self._constellation.satellites) != satellites:
            if time.time() - start > timeout:
                raise RuntimeError(
                    f"Timeout after {timeout}s while waiting for {satellites} satellites: "
                    f"only {len(self._constellation.satellites)} connected"
                )
            time.sleep(0.1)

    def reentry(self) -> None:
        """Shut down the controller and stop the background event loop."""
        self._stop_flag.set()
        self._thread.join(timeout=3)

    def _repr_pretty_(self, p: Any, _cycle: bool) -> None:
        nsat = len(self.constellation.satellites)
        p.text(f"AsyncCLIController(group='{self.group}') for {nsat} Satellites, current state is {self.state.name}")
