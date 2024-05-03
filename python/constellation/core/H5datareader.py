#!/usr/bin/env python3
"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0
"""

from pathlib import Path
import h5py


class H5DataReader:
    """Simple data reader for H5-files."""

    def __init__(self, file_name) -> None:
        self.file_name = file_name
        self.file = self._open_file(file_name)

    def __enter__(self):
        self.file = self._open_file(self.file_name)
        return self.file

    def __exit__(self, *args):
        self.file.close()

    def _open_file(self, file: str):
        file_path = Path(file)
        try:
            h5file = h5py.File(file_path)
        except Exception as exception:
            raise RuntimeError(
                f"Unable to open {file}: {str(exception)}",
            ) from exception
        return h5file

    def close(self):
        """Close H5-file."""
        self.file.close()

    def read_chunks(self, group: str, datasets: list, chunk_length: int):
        """ Read the file in chunks of length chunk_length"""
        def chunk_iterator():
            start = 0
            while start < len(datasets):
                data = []
                for i in range(chunk_length):
                    if start < len(datasets):
                        data.append(self.file[group][datasets[start]][:])
                        start += 1
                yield data
        return chunk_iterator()

    def groups(self):
        """Fetch all groups of H5-file."""
        return self._groups(self.file)

    def _groups(self, file):
        """Private method to fetch all datasets of H5-file."""
        groups = []
        for key in file.keys():
            if isinstance(file[key], h5py.Group):
                groups.append(key)
                groups.append(self._groups(file[key]))
        return groups

    def datasets(self):
        """Fetch a list of all datasets of H5-file."""
        return self._datasets(self.file)[0][2:]

    def _datasets(self, file):
        """Private method to fetch all datasets of H5-file."""
        datasets = []
        for key in file.keys():
            if isinstance(file[key], h5py.Group):
                datasets.append(self._datasets(file[key]))
            elif isinstance(file[key], h5py.Dataset):
                datasets.append(key)
        return datasets

    def sort_dataset_list(self):
        """ Returns a sorted list of all datasets """
        def sequence_number_sort(data_str):
            """Sort help function. Splits the datasetname and sort according
            to sequence_number """
            parts = data_str.split('_')
            numeric_part = int(parts[-1])
            return numeric_part

        dataset_list = self._datasets(self.file)
        sorted_dataset_list = sorted(dataset_list[0][2:],
                                     key=sequence_number_sort)
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
    print(
        "You can interact with your H5-file via the 'data' keyword"
    )

    embed()

    data.close()


if __name__ == "__main__":
    main()
