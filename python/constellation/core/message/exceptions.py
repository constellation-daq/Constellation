"""
SPDX-FileCopyrightText: 2025 DESY and the Constellation authors
SPDX-License-Identifier: EUPL-1.2

Exceptions for message classes
"""

from ..protocol import Protocol


class MessageDecodingError(RuntimeError):
    def __init__(self, reason: str):
        super().__init__(f"Error decoding message: {reason}")


class InvalidProtocolError(RuntimeError):
    def __init__(self, protocol: str):
        super().__init__(f"Invalid protocol identifier `{protocol}`")


class UnexpectedProtocolError(MessageDecodingError):
    def __init__(self, protocol_received: Protocol, protocol_expected: Protocol):
        super().__init__(
            f"Received protocol `{protocol_received.name}` does not match expected identifier `{protocol_expected.name}`"
        )
