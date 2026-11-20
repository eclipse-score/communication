# *******************************************************************************
# Copyright (c) 2026 Contributors to the Eclipse Foundation
#
# See the NOTICE file(s) distributed with this work for additional
# information regarding copyright ownership.
#
# This program and the accompanying materials are made available under the
# terms of the Apache License Version 2.0 which is available at
# https://www.apache.org/licenses/LICENSE-2.0
#
# SPDX-License-Identifier: Apache-2.0
# *******************************************************************************
"""Helpers that make SOME/IP messages for the TC8 conformance tests.

The helpers make REQUEST and REQUEST_NO_RETURN messages, and malformed messages.
"""

from someip_itf.codec import MessageType, build_someip


def build_request(
    service_id: int,
    method_id: int,
    client_id: int = 0x0001,
    session_id: int = 0x0001,
    interface_version: int = 0x00,
    payload: bytes = b"",
) -> bytes:
    """Build a SOME/IP REQUEST message."""
    return build_someip(
        service_id,
        method_id,
        client_id,
        session_id,
        interface_version,
        MessageType.REQUEST,
        payload=payload,
    )


def build_request_no_return(
    service_id: int,
    method_id: int,
    client_id: int = 0x0001,
    session_id: int = 0x0001,
    interface_version: int = 0x00,
    payload: bytes = b"",
) -> bytes:
    """Build a SOME/IP REQUEST_NO_RETURN message (no response)."""
    return build_someip(
        service_id,
        method_id,
        client_id,
        session_id,
        interface_version,
        MessageType.REQUEST_NO_RETURN,
        payload=payload,
    )


# ---------------------------------------------------------------------------
# Malformed messages
# ---------------------------------------------------------------------------


def build_truncated_message() -> bytes:
    """Return 7 raw bytes. This is one byte less than the minimum SOME/IP header of 8 bytes.

    The DUT must not crash when it receives this message.
    """
    # service_id=0x1234, method_id=0x0421, length=0x000000 (3 bytes, cut short)
    return b"\x12\x34\x04\x21\x00\x00\x00"


def build_wrong_protocol_version_request(
    service_id: int,
    method_id: int,
    client_id: int = 0x0001,
    session_id: int = 0x0001,
    interface_version: int = 0x00,
) -> bytes:
    """Build a valid REQUEST with protocol_version 0xFF.

    The DUT must reject or discard this message.
    """
    return build_someip(
        service_id,
        method_id,
        client_id,
        session_id,
        interface_version,
        MessageType.REQUEST,
        protocol_version=0xFF,
    )


def build_zero_length_message(
    service_id: int,
    method_id: int,
) -> bytes:
    """Return an 8-byte SOME/IP message with the length field set to 0.

    The length field must be at least 8. It counts the mandatory header fields after it
    (client_id, session_id, protocol_version, interface_version, message_type, return_code).
    This message does not have these fields.
    Thus, a length of 0 is an invalid header that is cut short. It is not a request with an empty payload.
    """
    return service_id.to_bytes(2, "big") + method_id.to_bytes(2, "big") + (0).to_bytes(4, "big")


def build_oversized_message(
    service_id: int,
    method_id: int,
    client_id: int = 0x0001,
    session_id: int = 0x0001,
    interface_version: int = 0x00,
) -> bytes:
    """Build a 16-byte packet with a length field of 0x7FF3.

    The UDP payload has only 16 bytes. Thus, the packet is much shorter than its length field.
    The DUT must not crash.
    """
    raw = build_request(
        service_id,
        method_id,
        client_id=client_id,
        session_id=session_id,
        interface_version=interface_version,
    )
    # The SOME/IP length field is bytes 4-7. It counts the bytes from byte 8.
    # A length of 0x7FF3 gives a body of 32755 bytes, but the body has only 8 bytes.
    return raw[:4] + b"\x00\x00\x7f\xf3" + raw[8:]


# ---------------------------------------------------------------------------
# Messages for the protocol behavior tests
# ---------------------------------------------------------------------------


def build_notification_as_request(
    service_id: int,
    method_id: int,
    client_id: int = 0x0001,
    session_id: int = 0x0001,
    interface_version: int = 0x00,
    payload: bytes = b"",
) -> bytes:
    """Build a SOME/IP packet with message_type=NOTIFICATION (0x02).

    A NOTIFICATION from the client to the server is not valid in SOME/IP.
    The DUT must not send a RESPONSE.
    """
    return build_someip(
        service_id,
        method_id,
        client_id,
        session_id,
        interface_version,
        MessageType.NOTIFICATION,
        payload=payload,
    )


def build_request_with_return_code(
    service_id: int,
    method_id: int,
    return_code: int,
    client_id: int = 0x0001,
    session_id: int = 0x0001,
    interface_version: int = 0x00,
    payload: bytes = b"",
) -> bytes:
    """Build a REQUEST with an explicit return_code byte value.

    In a REQUEST, the return_code must be E_OK (0x00).
    A value that is not zero tests the robustness of the DUT.
    """
    return build_someip(
        service_id,
        method_id,
        client_id,
        session_id,
        interface_version,
        MessageType.REQUEST,
        return_code=return_code & 0xFF,
        payload=payload,
    )
