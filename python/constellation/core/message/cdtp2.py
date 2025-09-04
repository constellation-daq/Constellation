"""
SPDX-FileCopyrightText: 2025 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2

Provides the message class for CDTP2
"""

from __future__ import annotations

from enum import IntEnum
from io import BytesIO
from typing import Any

import msgpack  # type: ignore[import-untyped]

from ..protocol import Protocol
from .exceptions import InvalidProtocolError, MessageDecodingError, UnexpectedProtocolError
from .msgpack_helpers import msgpack_unpack_to, msgpack_unpack_to_int_enum
from .multipart import MultipartMessage


class DataRecord:
    """Data record containing a tags and block with binary data"""

    def __init__(self, sequence_number: int, tags: dict[str, Any] | None = None) -> None:
        self._sequence_number = sequence_number
        self._tags = tags if tags is not None else {}
        self._blocks = list[bytes]()

    def add_block(self, data: bytes) -> None:
        """Add a block of data to the data record"""
        self._blocks.append(data)

    @property
    def sequence_number(self) -> int:
        return self._sequence_number

    @property
    def tags(self) -> dict[str, Any]:
        return self._tags

    @property
    def blocks(self) -> list[bytes]:
        return self._blocks

    def count_payload_bytes(self) -> int:
        return sum([len(block) for block in self._blocks])

    def pack(self, stream: BytesIO, packer: msgpack.Packer) -> None:
        stream.write(packer.pack_array_header(3))
        stream.write(packer.pack(self._sequence_number))
        stream.write(packer.pack(self._tags))
        stream.write(packer.pack_array_header(len(self._blocks)))
        for block in self._blocks:
            stream.write(packer.pack(block))

    @staticmethod
    def unpack(array: list[Any]) -> DataRecord:
        if len(array) != 3:
            raise MessageDecodingError("Data record array has wrong size")
        sequence_number, tags, blocks = array
        if type(sequence_number) is not int:
            raise MessageDecodingError("Sequence number is not an int")
        if type(tags) is not dict:
            raise MessageDecodingError("Record tags are not a dict")
        if type(blocks) is not list or any([type(block) is not bytes for block in blocks]):
            raise MessageDecodingError("Data blocks are not a list of bytes")
        data_record = DataRecord(sequence_number, tags)
        for block in blocks:
            data_record.add_block(block)
        return data_record


class CDTP2Message:
    """Message class for CDTP2"""

    class Type(IntEnum):
        """Enum describing the type of CDTP2 message"""

        """Data message"""
        DATA = 0x0

        """Begin-of-run message"""
        BOR = 0x1

        """End-of-run message"""
        EOR = 0x2

    def __init__(self, sender: str, type: CDTP2Message.Type) -> None:
        self._protocol = Protocol.CDTP2
        self._sender = sender
        self._type = type
        self._data_records = list[DataRecord]()

    def add_data_record(self, data_record: DataRecord) -> None:
        self._data_records.append(data_record)

    @property
    def sender(self) -> str:
        return self._sender

    @property
    def type(self) -> CDTP2Message.Type:
        return self._type

    @property
    def data_records(self) -> list[DataRecord]:
        return self._data_records

    def count_payload_bytes(self) -> int:
        return sum([data_record.count_payload_bytes() for data_record in self._data_records])

    def clear_data_records(self) -> None:
        self._data_records.clear()

    def assemble(self) -> MultipartMessage:
        stream = BytesIO()
        packer = msgpack.Packer(datetime=True)

        # Pack header
        stream.write(packer.pack(self._protocol.value))
        stream.write(packer.pack(self._sender))
        stream.write(packer.pack(self._type.value))
        # Pack payload
        stream.write(packer.pack_array_header(len(self._data_records)))
        for data_record in self._data_records:
            data_record.pack(stream, packer)

        return MultipartMessage([stream])

    @staticmethod
    def disassemble(frames: list[bytes]) -> CDTP2Message:
        unpacker = msgpack.Unpacker(timestamp=3)
        if len(frames) != 1:
            raise MessageDecodingError(f"Expected 1 frame, got {len(frames)}")

        # Unpack header
        unpacker.feed(frames[0])
        protocol = msgpack_unpack_to(unpacker, str)
        try:
            protocol = Protocol(protocol)
        except ValueError as e:
            raise InvalidProtocolError(protocol) from e
        if protocol is not Protocol.CDTP2:
            raise UnexpectedProtocolError(protocol, Protocol.CDTP2)
        sender = msgpack_unpack_to(unpacker, str)
        type = msgpack_unpack_to_int_enum(unpacker, CDTP2Message.Type)
        # Unpack array of data records
        data_records = []
        raw_data_records = msgpack_unpack_to(unpacker, list)
        for entry in raw_data_records:
            data_records.append(DataRecord.unpack(entry))

        # Assemble and return message
        msg = CDTP2Message(sender, type)
        for data_record in data_records:
            msg.add_data_record(data_record)
        return msg

    def __str__(self) -> str:
        return f"CDTP2 {self._type.name} message with {len(self._data_records)} data records"


class CDTP2BORMessage(CDTP2Message):
    """Message class for CDTP2 BOR messages"""

    def __init__(self, sender: str, user_tags: dict[str, Any], configuration: dict[str, Any]) -> None:
        super().__init__(sender, CDTP2Message.Type.BOR)
        self.add_data_record(DataRecord(0, user_tags))
        self.add_data_record(DataRecord(1, configuration))

    @staticmethod
    def cast(msg: CDTP2Message) -> CDTP2BORMessage:
        if msg.type != CDTP2Message.Type.BOR:
            raise MessageDecodingError("Not a BOR message")
        if len(msg._data_records) != 2:
            raise MessageDecodingError("Wrong number of data records, exactly two data records expected")
        msg.__class__ = CDTP2BORMessage
        return msg  # type: ignore[return-value]

    @property
    def user_tags(self) -> dict[str, Any]:
        return self._data_records[0].tags

    @property
    def configuration(self) -> dict[str, Any]:
        return self._data_records[1].tags


class CDTP2EORMessage(CDTP2Message):
    """Message class for CDTP2 EOR messages"""

    def __init__(self, sender: str, user_tags: dict[str, Any], run_metadata: dict[str, Any]) -> None:
        super().__init__(sender, CDTP2Message.Type.EOR)
        self.add_data_record(DataRecord(0, user_tags))
        self.add_data_record(DataRecord(1, run_metadata))

    @staticmethod
    def cast(msg: CDTP2Message) -> CDTP2EORMessage:
        if msg.type != CDTP2Message.Type.EOR:
            raise MessageDecodingError("Not a EOR message")
        if len(msg._data_records) != 2:
            raise MessageDecodingError("Wrong number of data records, exactly two data records expected")
        msg.__class__ = CDTP2EORMessage
        return msg  # type: ignore[return-value]

    @property
    def user_tags(self) -> dict[str, Any]:
        return self._data_records[0].tags

    @property
    def run_metadata(self) -> dict[str, Any]:
        return self._data_records[1].tags
