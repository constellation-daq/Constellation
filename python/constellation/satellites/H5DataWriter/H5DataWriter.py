"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2

Provides the class for the H5DataWriter satellite
"""

import datetime
import json
import os
import pathlib
from typing import Any

import h5py  # type: ignore[import-untyped]
import numpy as np

from constellation.core import __version__
from constellation.core.cmdp import MetricsType
from constellation.core.commandmanager import cscp_requestable
from constellation.core.message.cdtp2 import DataRecord
from constellation.core.message.cscp1 import CSCP1Message
from constellation.core.monitoring import schedule_metric
from constellation.core.receiver_satellite import ReceiverSatellite


class H5DataWriter(ReceiverSatellite):
    """Satellite which receives data via ZMQ and writes to HDF5."""

    def do_initializing(self, config: dict[str, Any]) -> str:
        """Initialize and configure the satellite."""
        self.output_directory = pathlib.Path(self.config["output_directory"])
        self.flush_interval: float = self.config.setdefault("flush_interval", 10.0)
        self.swmr_mode: bool = self.config.setdefault("allow_concurrent_reading", False)
        if self.swmr_mode and self.data_transmitters is None:
            raise RuntimeError("SWMR mode require list of known data transmitters")
        # Book keeping for swmr file indices and datasets
        self._swmr_idx: dict[str, list[int]] = {}
        self._swmr_dset: dict[str, tuple[h5py.Group, h5py.Dataset, h5py.Dataset, h5py.Dataset, h5py.Dataset]] = {}
        self._swmr_bor_sent: set[str] = set()
        self._swmr_mode_enabled: bool = False
        swmr_mode_str = "enabled" if self.swmr_mode else "disabled"
        return f"Initialized with flush interval {self.flush_interval}s and SWMR mode {swmr_mode_str}"

    def do_starting(self, run_identifier: str) -> str:
        self.last_flush = datetime.datetime.now()
        self._swmr_idx = {}
        self._swmr_bor_sent = set()
        self._swmr_mode_enabled = False
        self.outfile = self._open_file(f"data_{run_identifier}.h5")
        return f"Started run {run_identifier}"

    def do_stopping(self) -> str:
        self.outfile.close()
        self._swmr_mode_enabled = False
        return "Run stopped"

    def receive_bor(self, sender: str, user_tags: dict[str, Any], configuration: dict[str, Any]) -> None:
        grp = self.outfile.create_group(sender).create_group("BOR")
        # Add user tags
        grp.create_group("user_tags").attrs.update(self._attrs_convert(user_tags))
        # Add configuration
        grp.create_group("configuration").attrs.update(self._attrs_convert(configuration))
        # If in SWMR mode then we must create datasets in advance
        if self.swmr_mode:
            self._swmr_create_dataset(sender)

    def receive_eor(self, sender: str, user_tags: dict[str, Any], run_metadata: dict[str, Any]) -> None:
        if not self.swmr_mode:
            grp = self.outfile[sender].create_group("EOR")  # type: ignore
            # add user tags
            grp.create_group("user_tags").attrs.update(self._attrs_convert(user_tags))
            # add run metadata
            grp.create_group("run_metadata").attrs.update(self._attrs_convert(run_metadata))
        else:
            # Encode EOR as json buffer
            eor_user_tags = np.frombuffer(self._json_dumpb(user_tags), dtype=np.uint8)
            eor_run_meta = np.frombuffer(self._json_dumpb(run_metadata), dtype=np.uint8)
            dset_user_tags: h5py.Dataset = self.outfile[sender]["EOR"]["user_tags"]  # type: ignore
            dset_run_meta: h5py.Dataset = self.outfile[sender]["EOR"]["run_metadata"]  # type: ignore
            # Resize if needed
            if eor_user_tags.shape[0] > dset_user_tags.shape[0]:
                dset_user_tags.resize(eor_user_tags.shape[0], axis=0)
            if eor_run_meta.shape[0] > dset_run_meta.shape[0]:
                dset_run_meta.resize(eor_run_meta.shape[0], axis=0)
            # Write EOR
            dset_user_tags[0 : eor_user_tags.shape[0]] = eor_user_tags
            dset_run_meta[0 : eor_run_meta.shape[0]] = eor_run_meta

    def receive_data(self, sender: str, data_record: DataRecord) -> None:
        if self.swmr_mode:
            self._write_data_append(sender, data_record)
        else:
            self._write_data_create_dataset(sender, data_record)
        # time to flush data to file?
        if self.flush_interval > 0 and (datetime.datetime.now() - self.last_flush).total_seconds() > self.flush_interval:
            self.outfile.flush()
            self.last_flush = datetime.datetime.now()

    def _swmr_create_dataset(self, sender: str) -> None:
        """Creates all necessary datasets in advance as needed for SWMR mode."""
        grp: h5py.Group = self.outfile[sender]  # type: ignore[assignment]
        # 1D datasets (will store bytes) with reasonable chunks.
        # Will be resized to accommodate more data.
        dset = grp.create_dataset("data", (32000,), maxshape=(None,), dtype=np.uint8, chunks=(32000,))
        dset.attrs["CLASS"] = "DETECTOR_DATA"
        # Index dataset to store position inside the 'data' dataset.
        dset_idx = grp.create_dataset("data_idx", (100,), maxshape=(None,), dtype=np.uint64)
        dset_idx.attrs["CLASS"] = "INDEX"
        # Meta data, stored in binary format
        dset_meta = grp.create_dataset("meta", (32000,), maxshape=(None,), dtype=np.uint8, chunks=(32000,))
        dset_meta.attrs["CLASS"] = "META_DATA"
        # Index dataset to store position inside the 'meta' dataset.
        dset_meta_idx = grp.create_dataset("meta_idx", (100,), maxshape=(None,), dtype=np.uint64)
        dset_meta_idx.attrs["CLASS"] = "INDEX"
        # EOR as json
        eor_grp = grp.create_group("EOR")
        eor_grp.create_dataset("user_tags", (1000,), maxshape=(None,), dtype=np.uint8, chunks=(1000,))
        eor_grp.create_dataset("run_metadata", (1000,), maxshape=(None,), dtype=np.uint8, chunks=(1000,))

        # Add sender and reset book keeping indices
        self._swmr_bor_sent.add(sender)
        self._swmr_idx[sender] = [0, 0, 0]
        self._swmr_dset[sender] = (grp, dset, dset_idx, dset_meta, dset_meta_idx)

        # Check if ready to switch on SWMR mode
        if self._swmr_bor_sent == self.data_transmitters:
            # all satellites have sent BOR
            self.outfile.swmr_mode = True
            self._swmr_mode_enabled = True
            self.log.info("Enabled SWMR mode for file '%s'.", self.outfile.filename)

    def _write_data_create_dataset(self, sender: str, data_record: DataRecord) -> None:
        """Write payload of item into a new Dataset."""
        grp: h5py.Group = self.outfile[sender]  # type: ignore[assignment]

        # Create group for data record
        data_grp = grp.create_group(f"data_{data_record.sequence_number:09}")
        data_grp.attrs.update(self._attrs_convert(data_record.tags))

        # Extract numpy data type if specified
        dtype = data_record.tags.get("dtype", np.uint8)

        # Store each block as dataset
        for block_idx, block in enumerate(data_record.blocks):
            data_grp.create_dataset(f"block_{block_idx:02}", data=np.frombuffer(block, dtype=dtype), chunks=True)

    def _write_data_append(self, sender: str, data_record: DataRecord) -> None:
        """Write payload of item by appending existing Dataset."""
        # Get SWMR datasets from our cache
        grp, data_dset, dataidx_dset, meta_dset, metaidx_dset = self._swmr_dset[sender]
        data_idx, meta_idx, counts = self._swmr_idx[sender]

        # Append blocks into a single dataset of uint8 type
        block_lengths = [len(block) for block in data_record.blocks]
        data = np.empty(sum(block_lengths), dtype=np.uint8)
        idx = 0
        for block in data_record.blocks:
            data[idx : idx + len(block)] = np.frombuffer(block, dtype=np.uint8)
            idx += len(block)

        # Extract tags and add block lengths to it
        tags = data_record.tags
        tags["block_lengths"] = block_lengths

        # Check whether we need to resize
        new_idx = data_idx + data.shape[0]
        if new_idx >= data_dset.shape[0]:
            # extend dataset by at least 32kB to avoid frequent resizes.
            #
            # NOTE
            # this will likely affect performance to some extend. if effect is
            # significant, this should be a configuration variable.
            data_dset.resize(data_dset.shape[0] + max(32768, data.shape[0] * 10), axis=0)
        # store data
        data_dset[data_idx:new_idx] = data
        data_idx = new_idx

        if counts >= dataidx_dset.shape[0]:
            dataidx_dset.resize(dataidx_dset.shape[0] + 100, axis=0)
        dataidx_dset[counts] = data_idx

        meta = np.frombuffer(self._json_dumpb(tags), dtype=np.uint8)
        new_idx = meta_idx + meta.shape[0]
        if new_idx >= meta_dset.shape[0]:
            # extend meta dataset by at least 1kB to avoid frequent resizes
            #
            # NOTE
            # this will likely affect performance to some extend. if effect is
            # significant, this should be a configuration variable.
            meta_dset.resize(meta_dset.shape[0] + max(1024, meta.shape[0] * 20), axis=0)
        meta_dset[meta_idx:new_idx] = meta
        meta_idx = new_idx

        if counts >= metaidx_dset.shape[0]:
            metaidx_dset.resize(metaidx_dset.shape[0] + 100, axis=0)
        metaidx_dset[counts] = meta_idx

        counts += 1

        # update index values
        self._swmr_idx[sender] = [data_idx, meta_idx, counts]

    def _open_file(self, filename: str) -> h5py.File:
        """Open the hdf5 file and return the file object."""
        h5file = None
        self.log.info("Creating file %s", filename)

        # Create directory path
        directory = pathlib.Path(self.output_directory)
        try:
            os.makedirs(directory)
        except (FileExistsError, FileNotFoundError):
            self.log.info("Directory %s already exists", directory)
        except Exception as exception:
            raise RuntimeError(f"unable to create directory {directory}: {type(exception)} {str(exception)}") from exception

        # Create output file
        if os.path.isfile(directory / filename):
            self.log.critical("file already exists: %s", filename)
            raise RuntimeError(f"file already exists: {filename}")
        try:
            h5file = h5py.File(directory / filename, "w", libver="v110")
        except Exception as exception:
            self.log.critical("Unable to open %s: %s", filename, str(exception))
            raise RuntimeError(f"Unable to open {filename}: {str(exception)}") from exception
        self._add_metadata(h5file)
        return h5file

    def _add_metadata(self, outfile: h5py.File) -> None:
        """Add metadata such as version information to file."""
        metadata = {
            "constellation_version": __version__,
            "date_utc": datetime.datetime.now(datetime.UTC).isoformat(),
            "swmr_mode": self.swmr_mode,
        }
        grp = outfile.create_group(self.name)
        grp.attrs.update(self._attrs_convert(metadata))

    def _json_dumpb(self, obj: Any) -> bytes:
        """Dumps object to JSON with `str` as fallback and encoded to bytes"""
        return json.dumps(obj, separators=(",", ":"), default=str).encode("utf-8")

    def _attrs_convert(self, meta: dict[str, Any]) -> dict[str, Any]:
        """Convert dictionary values such that they can be stored as attributes"""

        def _convert(value: Any) -> Any:
            if isinstance(value, datetime.datetime):
                return str(value)
            return value

        return {key: _convert(value) for key, value in meta.items()}

    @schedule_metric("bool", MetricsType.LAST_VALUE, 5)
    def concurrent_reading_enabled(self) -> bool | None:
        """Concurrent reading status"""
        return self._swmr_mode_enabled

    @cscp_requestable
    def get_concurrent_reading_status(
        self,
        _request: CSCP1Message,
    ) -> tuple[str, Any, dict[str, Any]]:
        """Get if concurrent reading is enabled"""
        if self.swmr_mode:
            if self._swmr_mode_enabled:
                return "enabled", True, {}
            return "not yet enabled", False, {"bor_sent": list(self._swmr_bor_sent)}
        return "not enabled", False, {}
