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

"""Builds the SOME/IP-TP segments of an ETS request by hand.

A test can then send segments out of order, with overlaps, or too short.
"""

from __future__ import annotations

import struct
from typing import Final

from someip_itf.codec import MessageType
from someip_itf.ets.constants import ETS_MAJOR_VERSION, ETS_SERVER_UDP_PORT, ETS_SERVICE_ID
from someip_itf.ets.raw import ets_frame

TP_FLAG: Final = 0x20
"""Bit 5 of the message type marks a TP segment."""

TP_HEADER_SIZE: Final = 4
"""Size of the SOME/IP-TP header in bytes."""


def tp_header(byte_offset: int, more_segments: bool) -> bytes:
    """Build the 4-byte SOME/IP-TP header.

    The DUT reads bits 31..4 as the byte offset without a shift.
    Thus *byte_offset* goes directly into the top 28 bits. It must be 16-byte aligned.
    """
    assert byte_offset % 16 == 0, "TP offset must be a multiple of 16"
    value = (byte_offset & 0xFFFFFFF0) | (1 if more_segments else 0)
    return struct.pack(">I", value)


def tp_segment(
    method_id: int,
    session_id: int,
    byte_offset: int,
    segment_payload: bytes,
    more_segments: bool,
    *,
    client_id: int = 0x0001,
    proto_ver: int = 0x01,
    iface_ver: int = ETS_MAJOR_VERSION,
) -> bytes:
    """Build one SOME/IP-TP REQUEST segment: 16-byte header, 4-byte TP header, and payload."""
    return ets_frame(
        ETS_SERVICE_ID,
        method_id,
        tp_header(byte_offset, more_segments) + segment_payload,
        client_id=client_id,
        session_id=session_id,
        proto_ver=proto_ver,
        iface_ver=iface_ver,
        msg_type=int(MessageType.REQUEST) | TP_FLAG,
    )


def split_into_segments(content: bytes, chunk_size: int = 16):
    """Split application content into (offset, chunk, more_segments) tuples."""
    segments = []
    offset = 0
    while offset < len(content):
        chunk = content[offset : offset + chunk_size]
        more = (offset + len(chunk)) < len(content)
        segments.append((offset, chunk, more))
        offset += len(chunk)
    return segments


def send_message(sock, host_ip, method_id, session_id, content, chunk_size=16):
    """Send *content* as a full set of valid TP segments in the correct order."""
    for offset, chunk, more in split_into_segments(content, chunk_size):
        sock.sendto(
            tp_segment(method_id, session_id, offset, chunk, more),
            (host_ip, ETS_SERVER_UDP_PORT),
        )
