"""
SPDX-FileCopyrightText: 2025 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2

Configuration class
"""

from __future__ import annotations

import datetime
import enum
import functools
import os
import pathlib
import pprint
import re
from collections.abc import Callable
from io import BytesIO
from typing import Any, TypeVar, cast

import msgpack  # type: ignore[import-untyped]

T = TypeVar("T")


class ConfigurationError(RuntimeError):
    def __init__(self, what: str):
        super().__init__(what)


class MissingKeyError(ConfigurationError):
    def __init__(self, config: Section, key: str):
        super().__init__(f"Key `{config._prefix}{key}` does not exist")


class InvalidTypeError(ConfigurationError):
    def __init__(self, config: Section, key: str, vtype: str, ttype: str | None = None, reason: str | None = None):
        ttype_str = f"`{ttype}`" if ttype is not None else "target type"
        reason_str = f": {reason}" if reason is not None else ""
        super().__init__(
            f"Could not convert value of type `{vtype}` to {ttype_str} for key `{config._prefix}{key}`{reason_str}"
        )


class InvalidValueError(ConfigurationError):
    def __init__(self, config: Section, key: str, reason: str):
        super().__init__(f"Value of key `{config._prefix}{key}` is not valid: {reason}")


class InvalidUpdateError(ConfigurationError):
    def __init__(self, config: Section, key: str, reason: str):
        super().__init__(f"Failed to update value of key `{config._prefix}{key}`: {reason}")


def validate_dictionary_for_configuration(dictionary: Any, prefix: str = "") -> None:
    # Check that dict
    if not isinstance(dictionary, dict):
        raise TypeError("not a dictionary")
    keys_lc = set[str]()
    scalar_types = (bool, int, float, str, datetime.datetime)

    def convert_scalar(value: Any) -> Any:
        if not isinstance(value, datetime.datetime):
            if isinstance(value, datetime.time):
                # If time, combine with today date to get datetime
                value = datetime.datetime.combine(datetime.date.today(), value)
            elif isinstance(value, datetime.date):
                # If date, combine with midnight to get datetime
                value = datetime.datetime.combine(value, datetime.time(0, 0))
        if isinstance(value, datetime.datetime):
            # Convert timezone to UTC
            value = value.astimezone(datetime.UTC)
        return value

    for key, value in dictionary.items():
        # Check that keys are strings
        if not isinstance(key, str):
            raise TypeError(f"key `{prefix + str(key)}` not a string")
        # Check that keys are unique in lowercase
        key_lc = key.lower()
        prefixed_key = prefix + key_lc
        if key_lc in keys_lc:
            raise ValueError(f"key `{prefixed_key}` already present")
        keys_lc.add(key_lc)
        # List checks
        if isinstance(value, list):
            if value:
                # Check that list is homogeneous
                if not all(isinstance(element, type(value[0])) for element in value):
                    raise TypeError(f"array value of key `{prefixed_key}` not homogeneous")
                # Convert scalars in list
                value = [convert_scalar(x) for x in value]
                dictionary[key] = value
                # Check that elements are scalar types
                if not isinstance(value[0], scalar_types):
                    raise TypeError(f"elements of array value of key `{prefixed_key}` not scalar")
        # Dict checks
        elif isinstance(value, dict):
            # If dict, validate recursively
            validate_dictionary_for_configuration(value, prefixed_key + ".")
        # Scalar checks
        else:
            value = convert_scalar(value)
            dictionary[key] = value
            if not isinstance(value, scalar_types):
                raise TypeError(f"value of key `{prefixed_key}` not scalar")


def num_type(
    value: Any,
    return_type: type[int] | type[float] | None,
    min_val: int | float | None,
    max_val: int | float | None,
) -> int | float:
    """Return type function for numeric types"""
    # Check that value is a number
    if return_type is not None:
        if not isinstance(value, return_type):
            raise TypeError(f"not `{return_type.__name__}`")
    else:
        if not isinstance(value, (int, float)):
            raise TypeError("not `int` or `float`")
    # Check limits
    if min_val is not None:
        if value < min_val:
            raise ValueError(f"{value} is smaller than minimum value {min_val}")
    if max_val is not None:
        if value > max_val:
            raise ValueError(f"{value} is larger than maximum value {max_val}")
    return value


def absolute_path_type(value: Any, check_exists: bool) -> pathlib.Path:
    """Return type function for absolute paths"""
    # Convert path from string to pathlib path
    if not isinstance(value, str):
        raise ValueError("path needs to be a string")
    path = pathlib.Path(value)

    # If not a absolute path, make it an absolute path
    if not path.is_absolute():
        # Get current directory and append the relative path
        path = pathlib.Path(os.getcwd()).joinpath(path)

    # Check if the path exists
    exists = path.exists()
    if check_exists and not exists:
        raise ValueError(f"path `{path}` not found")

    # Normalize path if the path exists
    if exists:
        path = path.resolve()

    return path


def resolve_env_variable(match: re.Match[str]) -> str:
    """Resolve a variable name against environment variables"""
    var_name = match.group(1)
    default = match.group(2)

    value = os.environ.get(var_name)
    if value is not None:
        return value
    if default is not None:
        return default

    raise ValueError(f"Environment variable `{var_name}` not defined")


class Section:
    """Class to access a section in the configuration"""

    def __init__(self, prefix: str, dictionary: dict[str, Any]):
        self._prefix = prefix
        self._dictionary = dictionary
        self._used_keys = set[str]()
        self._section_tree = dict[str, Section]()
        # Convert dictionary to lowercase
        self._convert_lowercase()
        # Create configuration sections for any nested dictionaries in the dictionary
        self._create_section_tree()

    def __len__(self) -> int:
        return self._dictionary.__len__()

    def __contains__(self, key: str) -> bool:
        return self._dictionary.__contains__(key.lower())

    def __getitem__(self, key: str) -> Any:
        return self.get(key)

    def __repr__(self) -> str:
        return pprint.pformat(self._dictionary, sort_dicts=False)

    def has(self, key: str) -> bool:
        """Check if key is defined"""
        return self.__contains__(key)

    def count(self, keys: list[str]) -> int:
        """Check how many of the given keys are defined"""
        if not keys:
            raise ValueError("list of keys to count cannot be empty")
        found = 0
        for key in keys:
            if self.has(key):
                found += 1
        return found

    def set_default(self, key: str, value: Any) -> None:
        """Set default value for a key only if it is not defined yet"""
        self._dictionary.setdefault(key.lower(), value)

    def set_alias(self, new_key: str, old_key: str) -> None:
        """Set alias name for an already existing key"""
        if not self.has(old_key):
            return
        old_key_lc = old_key.lower()
        self._dictionary[new_key.lower()] = self._dictionary[old_key_lc]
        del self._dictionary[old_key_lc]
        # Note: we cannot warn here unfortunately due to the logging API

    def get(
        self,
        key: str,
        default_value: Any | None = None,
        return_type: type[T] | Callable[[Any], T] | None = None,
    ) -> T | Any:
        """Get value of a key in requested type"""
        key_lc = key.lower()

        if default_value is not None:
            self.set_default(key_lc, default_value)

        try:
            value = self._dictionary[key_lc]

            # Check / convert type
            value = self._check_type(key, value, return_type)

            # Check that no dictionary is returned
            if isinstance(value, dict):
                raise InvalidTypeError(
                    self,
                    key,
                    "Section",
                    reason="usage of `get_section` required",
                )

            self._mark_used(key_lc)
            return value
        except KeyError as e:
            raise MissingKeyError(self, key) from e

    def get_num(
        self,
        key: str,
        default_value: int | float | None = None,
        return_type: type[int] | type[float] | None = None,
        min_val: int | float | None = None,
        max_val: int | float | None = None,
    ) -> int | float:
        """Get value of key as number"""
        return self.get(
            key,
            default_value,
            functools.partial(
                num_type,
                return_type=return_type,
                min_val=min_val,
                max_val=max_val,
            ),
        )

    def get_int(
        self,
        key: str,
        default_value: int | None = None,
        min_val: int | None = None,
        max_val: int | None = None,
    ) -> int:
        """Get value of key as integer"""
        return cast(int, self.get_num(key, default_value, int, min_val, max_val))

    def get_float(
        self,
        key: str,
        default_value: float | None = None,
        min_val: float | None = None,
        max_val: float | None = None,
    ) -> float:
        """Get value of key as float"""
        return cast(float, self.get_num(key, default_value, float, min_val, max_val))

    def get_array(
        self,
        key: str,
        default_value: list[T] | None = None,
        element_type: type | Callable[[Any], T] | None = None,
    ) -> list[T]:
        """Get list with uniform type"""
        return self.get(
            key,
            default_value,
            lambda x: [self._check_type(f"{key}[{n}]", element, element_type) for n, element in enumerate(x)]
            if isinstance(x, list)
            else [self._check_type(key, x, element_type)],
        )

    def get_set(
        self,
        key: str,
        default_value: set[T] | None = None,
        element_type: type | Callable[[Any], T] | None = None,
    ) -> set[T]:
        """Get set with uniform type"""
        default_value_list = list(default_value) if default_value is not None else None
        return self.get(
            key,
            default_value_list,
            lambda x: {self._check_type(f"{key}['{element}']", element, element_type) for element in x},
        )

    def get_path(
        self,
        key: str,
        default_value: pathlib.Path | None = None,
        check_exists: bool = False,
    ) -> pathlib.Path:
        """Get value of key as path"""
        default_value_str = str(default_value) if default_value is not None else None
        return self.get(
            key,
            default_value_str,
            functools.partial(
                absolute_path_type,
                check_exists=check_exists,
            ),
        )

    def get_section(self, key: str, default_value: dict[str, Any] | None = None) -> Section:
        """Get nested configuration section"""
        key_lc = key.lower()
        if default_value is not None:
            # Set default value manually since dictionary needs to be inserted into tree
            if key_lc not in self._section_tree:
                if key_lc in self._dictionary:
                    raise InvalidTypeError(self, key, type(self._dictionary[key_lc]).__name__, "Section")
                self._dictionary[key_lc] = default_value
                self._section_tree[key_lc] = Section(f"{self._prefix}{key_lc}.", default_value)
        try:
            section = self._section_tree[key_lc]
            self._mark_used(key_lc)
            return section
        except KeyError as e:
            if key_lc in self._dictionary:
                raise InvalidTypeError(
                    self,
                    key,
                    type(self._dictionary[key_lc]).__name__,
                    "Section",
                ) from e
            raise MissingKeyError(self, key) from e

    def get_keys(self) -> list[str]:
        """Get the keys of the configuration section"""
        return list(self._dictionary.keys())

    def _remove_unused_entries(self) -> list[str]:
        """Remove unused entries from the configuration section"""
        unused_keys = list[str]()
        unused_keys_to_remove = list[str]()
        for key, value in self._dictionary.items():
            if isinstance(value, dict):
                # Sections require special handling
                sub_dict = self._section_tree[key]
                sub_unused_keys = sub_dict._remove_unused_entries()
                # Check if section was accessed at all
                if key in self._used_keys:
                    # If accessed, add unused sub keys recursively
                    unused_keys += sub_unused_keys
                else:
                    # If unused, add section key instead of sub keys and remove section entirely
                    unused_keys.append(self._prefix + key)
                    unused_keys_to_remove.append(key)
                    del self._section_tree[key]
            else:
                # Otherwise, check if unused and store key
                if key not in self._used_keys:
                    unused_keys.append(self._prefix + key)
                    unused_keys_to_remove.append(key)
        # Remove unused keys
        for key in unused_keys_to_remove:
            del self._dictionary[key]
        return unused_keys

    def _update(self, other: Section) -> None:
        """Update configuration section with values from another configuration section"""
        # Validate update beforehand
        self._validate_update(other)
        # Update values without validating again
        self._update_impl(other)

    def _validate_update(self, other: Section) -> None:
        for key, other_value in other._dictionary.items():
            # Check that key is also in self
            if key not in self._dictionary:
                raise InvalidUpdateError(self, key, "key does not exist in current configuration")
            value = self._dictionary[key]
            # Check that values has the same type
            if type(value) is not type(other_value):
                raise InvalidUpdateError(
                    self,
                    key,
                    f"cannot change type from `{type(value).__name__}` to `{type(other_value).__name__}`",
                )
            if isinstance(value, list):
                if value and other_value and type(value[0]) is not type(other_value[0]):
                    raise InvalidUpdateError(
                        self,
                        key,
                        f"cannot change type from `list[{type(value[0]).__name__}]` "
                        f"to `list[{type(other_value[0]).__name__}]`",
                    )
            elif isinstance(value, dict):
                # If dictionary, validate recursively
                self._section_tree[key]._validate_update(other._section_tree[key])

    def _update_impl(self, other: Section) -> None:
        for key, other_value in other._dictionary.items():
            value = self._dictionary[key]
            if isinstance(value, dict):
                # If dictionary, update recursively
                self._section_tree[key]._update_impl(other._section_tree[key])
            else:
                self._dictionary[key] = other_value

    def _convert_lowercase(self) -> None:
        # We need to keep the dict instance (pointer) identical, thus change in-place
        # Note: duplicate check already satisfied in validate_dictionary_for_configuration
        keys_to_lower = []
        for key in self._dictionary.keys():
            if key != key.lower():
                keys_to_lower.append(key)
        for key in keys_to_lower:
            self._dictionary[key.lower()] = self._dictionary[key]
            del self._dictionary[key]

    def _create_section_tree(self) -> None:
        for key, value in self._dictionary.items():
            # Check for dictionaries
            if isinstance(value, dict):
                # Create and store new nested configuration section
                sub_prefix = f"{self._prefix}{key}."
                self._section_tree[key] = Section(sub_prefix, value)

    def _mark_used(self, key: str) -> None:
        self._used_keys.add(key)

    def _check_type(self, key: str, value: Any, return_type: type | Callable[[Any], T] | None) -> T | Any:
        """Check (and convert) type for given return type"""
        if isinstance(return_type, type):
            # If return_type is type, check and throw
            if not isinstance(value, return_type):
                raise InvalidTypeError(self, key, type(value).__name__, return_type.__name__)
        elif return_type is not None:
            # Otherwise, use return_type function to convert value
            try:
                value = return_type(value)
            except ValueError as e:
                raise InvalidValueError(self, key, str(e)) from e
            except Exception as e:
                raise InvalidTypeError(self, key, type(value).__name__, reason=str(e)) from e
        return self._resolve_env(value)

    def _resolve_env(self, value: Any) -> Any:
        """Recursively resolve environment variables"""
        env_regex = re.compile(r"(?<!\\)\$\{(\w+)(?::-([^}]*))?\}")
        if isinstance(value, str):
            resolved = env_regex.sub(resolve_env_variable, value)
            return resolved.replace(r"\$", "$")
        elif isinstance(value, list):
            return [self._resolve_env(v) for v in value]
        elif isinstance(value, dict):
            return {k: self._resolve_env(v) for k, v in value.items()}
        else:
            return value


class ConfigurationGroup(enum.Enum):
    # All configuration key-value pairs, both user and internal
    ALL = enum.auto()
    # Configuration key-value pairs intended for framework users
    USER = enum.auto()
    # Configuration key-value pairs intended for internal framework usage
    INTERNAL = enum.auto()


class Configuration(Section):
    def __init__(self, root_dictionary: dict[str, Any] | None = None):
        self._root_dictionary = root_dictionary if root_dictionary is not None else {}
        validate_dictionary_for_configuration(self._root_dictionary)
        super().__init__("", self._root_dictionary)

    def to_string(self, group: ConfigurationGroup) -> str:
        def format_dict(dictionary: dict[str, Any], indent: int) -> str:
            out = ""
            indent_str = " " * indent
            for key, value in dictionary.items():
                if isinstance(value, dict):
                    out += f"{indent_str}{key}:{format_dict(value, indent + 2)}\n"
                else:
                    out += f"{indent_str}{key}: {value}\n"
            if out:
                out = out.rstrip()
                out = "\n" + out
            return out

        dictionary = self._dictionary
        if group == ConfigurationGroup.USER:
            dictionary = {key: value for key, value in self._dictionary.items() if not key.startswith("_")}
        elif group == ConfigurationGroup.INTERNAL:
            dictionary = {key: value for key, value in self._dictionary.items() if key.startswith("_")}

        return format_dict(dictionary, 2)

    def assemble(self) -> bytes:
        packer = msgpack.Packer(datetime=True)
        stream = BytesIO()
        stream.write(packer.pack(self._dictionary))
        return stream.getvalue()

    @staticmethod
    def disassemble(frame: bytes) -> Configuration:
        unpacker = msgpack.Unpacker(timestamp=3)
        unpacker.feed(frame)
        root_dictionary = unpacker.unpack()
        return Configuration(root_dictionary)
