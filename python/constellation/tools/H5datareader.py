#!/usr/bin/env python3
"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2
"""

import json
from pathlib import Path

import h5py
import numpy as np


class H5DataReader:
    """Simple data reader for H5-files."""

    def __init__(self, file_name) -> None:
        self.file_name = file_name
        self.file = self._open_file(file_name)
        self.swmr_mode = False

    def __enter__(self):
        self.file = self._open_file(self.file_name)
        return self.file

    def __exit__(self, *args):
        self.file.close()

    def _open_file(self, file: str):
        file_path = Path(file)
        while True:
            try:
                h5file = h5py.File(file_path, "r", libver="latest", swmr=True)
                break
            except Exception as exception:

                raise RuntimeError(
                    f"Unable to open {file}: {str(exception)}",
                ) from exception

        return h5file

    def close(self):
        """Close H5-file."""
        self.file.close()

    def get_swmr_mode(self):
        """Fetch the swmr status of the file"""
        try:
            self.swmr_mode = self.file["H5DataWriter.DataWriter"]["swmr_mode"][()]
        except KeyError:
            self.swmr_mode = False
        return self.swmr_mode

    def read_chunks_swmr(self, group: str):
        """Read the file in chunks in swmr mode"""

        class MyChunkIteratorSWMR:
            def __init__(self, h5file, group):
                self.file = h5file
                self.group = group
                self.prev_idx = 0
                self.i = 0
                self._setup_dtype()

            def _setup_dtype(self):
                meta_idx = self.file[self.group]["meta_idx"]
                if meta_idx[0] == 0:
                    self.dtype = "int16"
                else:
                    meta_bytes = self.file[self.group]["meta"][0 : meta_idx[0]]
                    meta_str = meta_bytes.tobytes().decode("utf-8")
                    m = json.loads(meta_str)
                    self.dtype = m.get("dtype", "int16")

            def __iter__(self):
                return self

            def __next__(self):
                self.file[self.group]["data_idx"].id.refresh()
                data_idx = self.file[self.group]["data_idx"]
                self.file[self.group]["data"].id.refresh()
                data = self.file[self.group]["data"]

                if self.i >= len(data_idx):
                    raise StopIteration

                idx = data_idx[self.i]

                if idx == 0:
                    raise StopIteration

                chunk = [np.array(data[self.prev_idx : idx]).view(self.dtype)]
                self.prev_idx = idx
                self.i += 1
                return chunk

        return MyChunkIteratorSWMR(self.file, group)

    def read_chunks(self, group: str, datasets: list, chunk_length: int):
        """Read the file in chunks of length chunk_length"""

        def chunk_iterator():
            # Total number of datasets
            num_datasets = len(datasets)

            # Iterate through the datasets in steps of chunk_length
            for start in range(0, num_datasets, chunk_length):
                end = min(start + chunk_length, num_datasets)
                chunk = [self.file[group][datasets[i]][:] for i in range(start, end)]
                yield chunk

        return chunk_iterator()

    def groups(self):
        """Fetch all group names of H5-file."""
        return self._groups(self.file)

    def _groups(self, file):
        """Private method to fetch all group names of H5-file."""
        groups = []
        for key in file.keys():
            if isinstance(file[key], h5py.Group):
                groups.append(key)
        return groups

    def get_EOR_payload(self, group):
        """Fetch the payload of the EOR for the group"""
        if self.swmr_mode:
            self.file[group]["EOR"].id.refresh()
        return self.file[group]["EOR"]

    def get_BOR_payload(self, group):
        """Fetch the payload of the BOR for the group"""
        return self.file[group]["BOR"]

    def datasets(self, group):
        """Fetch a list of all dataset names of H5-file."""
        return self._datasets(self.file, group)[0]

    def _datasets(self, file, group):
        """Private method to fetch all dataset names of H5-file."""
        datasets = []
        # Access the dataset group
        dataset_group = file[group]
        for dataset_name, dataset in dataset_group.items():
            datasets.append(dataset_name)
        return datasets

    def sort_dataset_list(self, group):
        """Returns a sorted list of all datasets"""

        def sequence_number_sort(data_str):
            """Sort help function. Splits the datasetname and sort according
            to sequence_number"""
            if data_str != "BOR" and data_str != "EOR":
                parts = data_str.split("_")
                numeric_part = int(parts[-1])
            else:
                numeric_part = 0
            return numeric_part

        dataset_list = self._datasets(self.file, group)
        sorted_dataset_list = sorted(dataset_list, key=sequence_number_sort)
        return sorted_dataset_list


# -------------------------------------------------------------------------


def main(args=None):
    import argparse

    from IPython import embed

    parser = argparse.ArgumentParser()
    parser.add_argument("--file-name")
    args = parser.parse_args(args)

    data = H5DataReader(file_name=args.file_name)

    print("\nWelcome to the Constellation CLI IPython Reader for H5 files!\n")
    print("You can interact with your H5-file via the 'data' keyword")

    embed()

    data.close()


if __name__ == "__main__":
    main()
