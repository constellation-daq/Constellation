"""
SPDX-FileCopyrightText: 2025 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2

A base module for a Constellation Satellite that receives data.
"""

import pathlib
import random
import shutil
import string
import time
from enum import Enum
from typing import Any

from .cdtp import DataReceiver, RecvTimeoutError
from .chirp import CHIRPServiceIdentifier, get_uuid
from .chirpmanager import DiscoveredService, chirp_callback
from .configuration import Configuration, InvalidValueError
from .error import debug_log, handle_error
from .message.cdtp2 import DataRecord
from .monitoring import schedule_metric
from .protocol.cscp1 import is_valid_canonical_name
from .satellite import Satellite


class FileConflictStrategy(Enum):
    """Strategy for handling conflicts in file creation"""

    ERROR = "ERROR"
    OVERWRITE = "OVERWRITE"
    RENAME = "RENAME"


class ReceiverSatellite(Satellite):
    """Constellation Satellite which can receive data via CDTP"""

    def __init__(self, *args: Any, **kwargs: Any):
        self._drc: DataReceiver | None = None
        self._file_conflict_strategy = FileConflictStrategy.RENAME

        # Initialize satellite
        super().__init__(*args, **kwargs)

        self.log_cdtp = self.get_logger("DATA")
        self.register_metric("OUTPUT_FILE", "", "Current output file path. Updated when changed.")

    def _pre_initializing_hook(self, config: Configuration) -> None:
        """Configure values specific for all ReceiverSatellite-type classes."""
        super()._pre_initializing_hook(config)

        config_data = config.get_section("_data", {})
        self._file_conflict_strategy = config_data.get_enum(
            FileConflictStrategy, "file_conflict_strategy", FileConflictStrategy.RENAME
        )
        self.log_cdtp.info("Solution strategy for file name conflicts: %s", self._file_conflict_strategy.name)

        data_transmitters: None | set[str] = None
        if "receive_from" in config_data:
            data_transmitters = config_data.get_set("receive_from", element_type=str)
            for data_transmitter in data_transmitters:
                if not is_valid_canonical_name(data_transmitter):
                    raise InvalidValueError(
                        config_data, "receive_from", f"`{data_transmitter}` is not a valid canonical name"
                    )
        # Create data receiver
        self._drc = DataReceiver(
            self.context, self.log_cdtp, self.receive_bor, self.receive_data, self.receive_eor, data_transmitters
        )
        # EOR timeout
        self._drc.eor_timeout = config_data.get_int("eor_timeout", 10, min_val=1)
        self.log_cdtp.debug("Timeout for EOR messages is %ss", self._drc.eor_timeout)

    def _pre_launching_hook(self) -> None:
        """Check the available data transmitters against the configured list."""
        super()._pre_launching_hook()

        # Always request data services
        self.request(CHIRPServiceIdentifier.DATA)

        # If this receiver should connect to all available transmitters, return
        if self.data_transmitters is None:
            return

        # Wait until all data services are discovered
        timeout = 3.0
        missing_transmitters = self.data_transmitters
        while timeout > 0.0 and len(missing_transmitters) > 0:
            for dataservice in self.get_discovered(CHIRPServiceIdentifier.DATA):
                missing_transmitters = {h for h in missing_transmitters if get_uuid(h) != dataservice.host_uuid}

            time.sleep(0.1)
            timeout -= 0.1

        if len(missing_transmitters) > 0:
            raise RuntimeError(f"The requested data transmitters {', '.join(missing_transmitters)} are not available")

    def _pre_run_hook(self) -> None:
        """Hook run immediately before `do_run()` is called.

        Start the data receiver and requests DATA services via CHIRP.
        """
        # Emit metric for received bytes
        self.stat("rx_bytes", 0)

        # Start data receiver
        assert self._drc is not None
        self._drc.start_receiving()
        # Request data services
        self.request(CHIRPServiceIdentifier.DATA)
        # Add data services already discovered
        for service in self.get_discovered(CHIRPServiceIdentifier.DATA):
            self._drc.add_sender(service)

    @handle_error
    @debug_log
    def _wrap_stop(self, payload: Any) -> Any:
        # Stop data receiver
        assert self._drc is not None
        self._drc.stop_receiving()

        # Emit metric for received bytes
        self.stat("rx_bytes", self._drc.bytes_received)

        return super()._wrap_stop(payload)

    @handle_error
    @debug_log
    def _wrap_interrupt(self, payload: Any) -> str:
        # Stop data receiver but do not throw if not all EOR messages received
        assert self._drc is not None
        if self._drc.running:
            try:
                self._drc.stop_receiving()
            except RecvTimeoutError as e:
                self.log_cdtp.warning(str(e))

        res: str = super()._wrap_interrupt(payload)
        return res

    @handle_error
    @debug_log
    def _wrap_failure(self, payload: Any) -> str:
        # Stop data receiver pull thread directly
        if self._drc is not None and self._drc.running:
            self._drc._stop_pull_thread()

        res: str = super()._wrap_failure(payload)
        return res

    @chirp_callback(CHIRPServiceIdentifier.DATA)
    def _add_sender_callback(self, service: DiscoveredService) -> None:
        """Callback method for connecting to data service."""
        if self._drc is not None:
            if not service.alive:
                self._drc.remove_sender(service)
            else:
                self._drc.add_sender(service)

    def validate_output_directory(self, path: pathlib.Path | str) -> pathlib.Path:
        """Validate the output directory

        This method checks if the directory exists and is a directory, creates all parent directories if necessary, and
        registers a disk space metric for the path. Returns prospective output directory as absolute path
        """
        try:
            directory = pathlib.Path(path)
            directory.mkdir(parents=True, exist_ok=True)
            directory = directory.resolve()
        except OSError as e:
            raise RuntimeError(f"Issue with output directory: {e}") from e
        if not directory.is_dir():
            raise RuntimeError(f"Requested output directory {directory} is not a directory")
        self._register_diskspace_metric(directory)
        return directory

    def validate_output_file(self, path: pathlib.Path | str, file_name: str, ext: None | str = None) -> pathlib.Path:
        """Create the output file path

        Create the final output file path from output directory, file name and extension and validates access. Returns the
        absolute path to the file.
        """
        file = pathlib.Path(path) / file_name
        try:
            # Replace extension if desired
            if ext:
                file = file.with_suffix("." + ext.removeprefix("."))

            # Create all the required directories
            file.parent.mkdir(parents=True, exist_ok=True)

            # Handle file name conflicts
            if file.exists():
                if self._file_conflict_strategy == FileConflictStrategy.ERROR:
                    raise RuntimeError(f"Overwriting of existing file {file} denied")
                elif self._file_conflict_strategy == FileConflictStrategy.RENAME:
                    suffix = "".join(random.choices(string.digits + string.ascii_lowercase, k=8))
                    file = file.with_name(f"{file.stem}_{suffix}{file.suffix}")
                    self.log_cdtp.warning("Target file exists, redirecting output to file %s", file)
                else:
                    self.log_cdtp.warning("File %s exists and will be overwritten", file)
                    file.unlink()

            # Create the file exclusively to check if it can be accessed
            with open(file, "x"):
                pass
            file = file.resolve()
        except FileExistsError as e:
            raise RuntimeError(f"Overwriting of existing file {file} denied") from e
        except OSError as e:
            raise RuntimeError(f"Issue with output path: {e}") from e

        self._register_diskspace_metric(file)

        # Send metric with new output file path
        self.stat("OUTPUT_FILE", str(file))

        return file

    def _register_diskspace_metric(self, path: pathlib.Path) -> None:
        """Register or update the disk space metric for the given path"""

        def diskspace_free() -> int | None:
            try:
                available_mib = shutil.disk_usage(path).free >> 20
            except OSError as e:
                self.log_cdtp.debug(str(e))
                return None
            # Less than 10GiB disk space - let's warn the user via logs!
            if available_mib >> 10 < 3:
                self.log_cdtp.critical("Available disk space critically low, %sMiB left", available_mib)
            elif available_mib >> 10 < 10:
                self.log_cdtp.warning("Available disk space low, %sMiB left", available_mib)
            return available_mib

        self.register_scheduled_metric(
            "DISKSPACE_FREE", "MiB", "Available disk space at the target location of the output file", 5, diskspace_free
        )

    @schedule_metric("B", 5)
    def rx_bytes(self) -> int | None:
        """Amount of bytes received from all transmitters during current run"""
        if self._drc is not None and self._drc.running:
            return self._drc.bytes_received
        return None

    @property
    def data_transmitters(self) -> set[str] | None:
        if self._drc is not None:
            return self._drc.data_transmitters
        return None

    def receive_bor(self, sender: str, user_tags: dict[str, Any], configuration: dict[str, Any]) -> None:
        """Receive begin-of-run (BOR)"""
        raise NotImplementedError()

    def receive_data(self, sender: str, data_record: DataRecord) -> None:
        """Receive data"""
        raise NotImplementedError()

    def receive_eor(self, sender: str, user_tags: dict[str, Any], run_metadata: dict[str, Any]) -> None:
        """Receive end-of-run (EOR)"""
        raise NotImplementedError()

    def do_run(self) -> str | None:
        """Run loop

        NOTE: This must not be overridden by receiver satellite implementations!
        """
        assert self._drc is not None
        while not self.stop_requested():
            # Check and rethrow exception from BasePool
            self._drc.check_exception()
            # Wait a bit to avoid hot loop
            time.sleep(0.1)
        return "Finished run"
