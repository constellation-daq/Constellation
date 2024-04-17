#!/usr/bin/env python3
"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: CC-BY-4.0

Base module for Constellation Satellites that receive data.
"""

import h5py
from pathlib import Path


class H5DataReader:
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
        self.file.close()

    def read_chunks(self, group: str, dset: str, start: int, stop: int):
        return self.file[group][dset].chunks[start:stop]

    def groups(self):
        return self._groups(self.file)

    def _groups(self, file):
        groups = []
        for key in self.file.keys():
            if isinstance(key, h5py.Group):
                groups.append(self._groups(key))
            else:
                groups.append(key)


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
        "You can interact with your H5-file via the 'data' keyword"  # NOTE: Needs more features
    )

    embed()

    data.close()


if __name__ == "__main__":
    main()
