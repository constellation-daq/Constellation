"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2

Provides the class for the H5DataWriter satellite
"""

import datetime
import json
import os
import pathlib
from typing import Any, Tuple

import h5py  # type: ignore[import-untyped]
import numpy as np

from constellation.core import __version__
from constellation.core.cdtp import CDTPMessage
from constellation.core.cmdp import MetricsType
from constellation.core.commandmanager import cscp_requestable
from constellation.core.cscp import CSCPMessage
from constellation.core.datareceiver import DataReceiver
from constellation.core.monitoring import schedule_metric
from constellation.core.fsm import SatelliteState


class H5DataWriter(DataReceiver):
    """Satellite which receives data via ZMQ and writes to HDF5.

    Supported configuration options:
    - flush_interval (float): how often to write out the file (in seconds).
    - allow_concurrent_reading (bool): whether or not to allow reading while writing (SWMR mode).
    """

    def do_initializing(self, config: dict[str, Any]) -> str:
        """Initialize and configure the satellite."""
        super().do_initializing(config)
        # how often will the file be flushed? Negative values for 'at the end of
        # the run'
        self.flush_interval: float = self.config.setdefault("flush_interval", 10.0)
        self.swmr_mode: bool = self.config.setdefault("allow_concurrent_reading", False)
        # book keeping for swmr file indices
        self._swmr_idx: dict[str, list[int]] = {}
        self._swmr_mode_enabled: bool = False
        return "Configured all values"

    def do_run(self, run_identifier: str) -> str:
        """Handle the data enqueued by the ZMQ Poller."""
        self.last_flush = datetime.datetime.now()
        self._swmr_idx = {}
        self._swmr_mode_enabled = False
        return super().do_run(run_identifier)

    @schedule_metric("bool", MetricsType.LAST_VALUE, 1)
    def concurrent_reading_enabled(self) -> bool:
        if self.fsm.current_state_value in [
            SatelliteState.NEW,
            SatelliteState.ERROR,
            SatelliteState.DEAD,
            SatelliteState.initializing,
            SatelliteState.reconfiguring,
        ]:
            return None
        return self._swmr_mode_enabled

    @cscp_requestable
    def get_concurrent_reading_status(
        self,
        _request: CSCPMessage,
    ) -> Tuple[str, None, None]:
        if self._swmr_mode_enabled:
            return "enabled", None, None
        return "not (yet) enabled", None, None

    def _write_EOR(self, outfile: h5py.File, item: CDTPMessage) -> None:
        """Write EOR to file"""
        if not self.swmr_mode:
            grp = outfile[item.name].create_group("EOR")
            # add meta information as attributes
            grp.update(item.payload)
        else:
            # encode EOR as bytes
            eor = np.frombuffer(json.dumps(item.payload).encode("utf-8"), dtype=np.uint8)
            dset = outfile[item.name]["EOR"]
            # resize if needed
            if eor.shape[0] > dset.shape[0]:
                dset.resize(eor.shape[0], axis=0)
            dset[0 : eor.shape[0]] = eor
        self.log.info(
            "Wrote EOR packet from %s on run %s",
            item.name,
            self.run_identifier,
        )

    def _write_BOR(self, outfile: h5py.File, item: CDTPMessage) -> None:
        """Write BOR to file"""
        grp = outfile.create_group(item.name).create_group("BOR")
        # add payload dict information as attributes
        grp.update(item.payload)
        self.log.info(
            "Wrote BOR packet from %s on run %s",
            item.name,
            self.run_identifier,
        )
        # if in SWMR mode then we must create datasets in advance
        if self.swmr_mode:
            self._swmr_create_dataset(outfile, item)

    def _swmr_create_dataset(self, outfile: h5py.File, item: CDTPMessage) -> None:
        """Creates all necessary datasets in advance as needed for SWMR mode."""
        grp = outfile[item.name]
        # 1D datasets (will store bytes) with reasonable chunks.
        # Will be resized to accommodate more data.
        dset = grp.create_dataset("data", (32000,), maxshape=(None,), dtype=np.dtype("uint8"), chunks=(32000,))
        dset.attrs["CLASS"] = "DETECTOR_DATA"
        # Index dataset to store position inside the 'data' dataset.
        dset = grp.create_dataset("data_idx", (100,), maxshape=(None,), dtype=np.dtype("int"))
        dset.attrs["CLASS"] = "INDEX"
        # Meta data, stored in binary format
        dset = grp.create_dataset("meta", (32000,), maxshape=(None,), dtype=np.dtype("uint8"), chunks=(32000,))
        dset.attrs["CLASS"] = "META_DATA"
        # Index dataset to store position inside the 'meta' dataset.
        dset = grp.create_dataset("meta_idx", (100,), maxshape=(None,), dtype=np.dtype("int"))
        dset.attrs["CLASS"] = "INDEX"
        # EOR
        dset = grp.create_dataset("EOR", (1000,), maxshape=(None,), dtype=np.dtype("uint8"), chunks=(1000,))
        # reset book keeping indices
        self._swmr_idx[item.name] = [0, 0, 0]
        # ready to switch on SWMR?
        if self.swmr_mode and len(self.active_satellites) == len(self._pull_interfaces):
            # all satellites have sent BOR
            outfile.swmr_mode = True
            self._swmr_mode_enabled = True
            self.log.info("Enabled SWMR mode for file '%s'.", outfile.filename)

    def _write_data(self, outfile: h5py.File, item: CDTPMessage) -> None:
        """Write data into HDF5 format

        Format: h5file -> Group (name) ->   BOR Dataset
                                            Single Concatenated Dataset
                                            EOR Dataset

        Writes data to file by concatenating item.payload to dataset inside group name.
        """
        # Check if group already exists.
        try:
            grp = outfile[item.name]
        except KeyError as exc:
            # late joiners
            self.log.warning("%s sent data without BOR.", item.name)
            if not outfile.swmr_mode:
                # SWMR not (yet) active
                self.active_satellites.append(item.name)
                grp = outfile.create_group(item.name)
                if self.swmr_mode:
                    self._swmr_create_dataset(outfile, item)
            else:
                # This is a safety net against possible data corruption only.
                # Would need a serious bug to have SWMR enabled without having
                # seen all BOR!
                self.log.error("%s sent data without BOR, but SWMR mode already active.", item.name)
                raise RuntimeError(f"{item.name} sent data without BOR, but SWMR mode already active.") from exc

        if item.name not in self.active_satellites:
            self.log.warning(
                "%s sent data but is no longer assumed active (EOR received)",
                item.name,
            )

        if self.swmr_mode:
            self._write_data_append(grp, item)
        else:
            self._write_data_create_dataset(grp, item)

        # time to flush data to file?
        if self.flush_interval > 0 and (datetime.datetime.now() - self.last_flush).total_seconds() > self.flush_interval:
            outfile.flush()
            self.last_flush = datetime.datetime.now()

    def _write_data_create_dataset(self, grp: h5py.Group, item: CDTPMessage) -> None:
        """Write payload of item into a new Dataset."""
        title = f"data_{self.run_identifier}_{item.sequence_number:09}"

        if isinstance(item.payload, bytes):
            # interpret bytes as array of uint8 if nothing else was specified in the meta
            payload = np.frombuffer(item.payload, dtype=item.meta.get("dtype", np.uint8))
        elif isinstance(item.payload, list):
            payload = np.array(item.payload)
        elif item.payload is None:
            # empty payload -> empty array of bytes
            payload = np.array([], dtype=np.uint8)
        else:
            raise TypeError(f"Cannot write payload of type '{type(item.payload)}'")

        dset = grp.create_dataset(
            title,
            data=payload,
            chunks=True,
        )
        dset.attrs["CLASS"] = "DETECTOR_DATA"
        dset.attrs.update(item.meta)

    def _write_data_append(self, grp: h5py.Group, item: CDTPMessage) -> None:
        """Write payload of item by appending existing Dataset."""
        data_dset = grp["data"]
        dataidx_dset = grp["data_idx"]
        meta_dset = grp["meta"]
        metaidx_dset = grp["meta_idx"]
        data_idx, meta_idx, counts = self._swmr_idx[item.name]

        # make sure that we have an numpy array of uint8:
        if isinstance(item.payload, bytes):
            # use bytes as is
            payload = np.frombuffer(item.payload, dtype=np.uint8)
        elif isinstance(item.payload, list):
            # convert to bytes
            payload = np.frombuffer(bytes(item.payload), dtype=np.uint8)
        elif item.payload is None:
            # empty payload -> empty array of bytes
            payload = np.array([], dtype=np.uint8)
        else:
            raise TypeError(f"Cannot write payload of type '{type(item.payload)}'")

        # check whether we need to resize
        new_idx = data_idx + payload.shape[0]
        if new_idx >= data_dset.shape[0]:
            # extend dataset by at least 32kB to avoid frequent resizes.
            #
            # NOTE
            # this will likely affect performance to some extend. if effect is
            # significant, this should be a configuration variable.
            data_dset.resize(data_dset.shape[0] + max(32768, payload.shape[0] * 10), axis=0)
        # store data
        data_dset[data_idx:new_idx] = payload
        data_idx = new_idx

        if counts >= dataidx_dset.shape[0]:
            dataidx_dset.resize(dataidx_dset.shape[0] + 100, axis=0)
        dataidx_dset[counts] = data_idx

        meta = np.frombuffer(json.dumps(item.meta).encode("utf-8"), dtype=np.uint8)
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
        self._swmr_idx[item.name] = [data_idx, meta_idx, counts]

    def _open_file(self, filename: pathlib.Path) -> h5py.File:
        """Open the hdf5 file and return the file object."""
        h5file = None
        if os.path.isfile(filename):
            self.log.critical("file already exists: %s", filename)
            raise RuntimeError(f"file already exists: {filename}")

        self.log.info("Creating file %s", filename)
        # Create directory path.
        directory = pathlib.Path(self.output_path)  # os.path.dirname(filename)
        try:
            os.makedirs(directory)
        except (FileExistsError, FileNotFoundError):
            self.log.info("Directory %s already exists", directory)
        except Exception as exception:
            raise RuntimeError(
                f"unable to create directory {directory}: \
                {type(exception)} {str(exception)}"
            ) from exception
        try:
            h5file = h5py.File(directory / filename, "w", libver="v110")
        except Exception as exception:
            self.log.critical("Unable to open %s: %s", filename, str(exception))
            raise RuntimeError(
                f"Unable to open {filename}: {str(exception)}",
            ) from exception
        self._add_metadata(h5file)
        return h5file

    def _close_file(self, outfile: h5py.File) -> None:
        """Close the filehandler"""
        outfile.close()
        self._swmr_mode_enabled = False

    def _add_metadata(self, outfile: h5py.File) -> None:
        """Add metadata such as version information to file."""
        grp = outfile.create_group(self.name)
        grp["constellation_version"] = __version__
        grp["date_utc"] = datetime.datetime.now(datetime.timezone.utc).isoformat()
        grp["swmr_mode"] = self.swmr_mode
