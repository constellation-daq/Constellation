"""
SPDX-FileCopyrightText: 2024 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2

This module provides the useful utilities.
"""

import pprint
from collections.abc import Iterator, MutableMapping
from dataclasses import dataclass
from typing import Generic, TypeVar

T = TypeVar("T")


@dataclass
class cased_entry(Generic[T]):
    cased_key: str
    value: T


class case_insensitive_dict(MutableMapping[str, T]):
    def __init__(self, dictionary: dict[str, T] | None = None):
        # Map lower-cased key to tuple of cased key and value
        self._dictionary: dict[str, cased_entry[T]] = {}
        if dictionary is not None:
            self.update(dictionary)

    def __getitem__(self, key: str, /) -> T:
        return self._dictionary.__getitem__(key.casefold()).value

    def __setitem__(self, key: str, value: T, /) -> None:
        return self._dictionary.__setitem__(key.casefold(), cased_entry(key, value))

    def __delitem__(self, key: str, /) -> None:
        return self._dictionary.__delitem__(key.casefold())

    def __iter__(self) -> Iterator[str]:
        return (entry.cased_key for entry in self._dictionary.values())

    def __len__(self) -> int:
        return self._dictionary.__len__()

    def __repr__(self) -> str:
        return pprint.pformat(self.as_dict(), sort_dicts=False)

    def as_dict(self) -> dict[str, T]:
        return {entry.cased_key: entry.value for entry in self._dictionary.values()}
