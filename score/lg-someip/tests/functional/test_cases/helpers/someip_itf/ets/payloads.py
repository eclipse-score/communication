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

"""Packs and unpacks the payloads of the ETS methods and events in the DUT service layout."""

from __future__ import annotations

import struct

from someip_itf.ets.payload_types import uINT8ArrayArray

# echoCommonDatatypes (method 35) has bool, u8, u16, u32, i8, i16, i32, float: 19 bytes.
_COMMON_DATATYPES_IN_FMT = ">?BHLbhlf"


def pack_array(values: list[int]) -> bytes:
    """Pack a uint8 dynamic array: a big-endian uint32 length, then the bytes."""
    return struct.pack(">I", len(values)) + bytes(values)


def unpack_array(payload: bytes) -> list[int]:
    """Return the bytes of a uint8 dynamic array. If the payload is shorter than the length field, return empty."""
    if len(payload) < 4:
        return []
    (n,) = struct.unpack(">I", payload[:4])
    return list(payload[4 : 4 + n])


def unpack_version(payload: bytes) -> tuple[int, int]:
    """Return the major uint8 and minor uint32 of a version field."""
    return struct.unpack(">BI", payload[:5])


def minsize_payload(size: int) -> bytes:
    """Build an echoUINT8ArrayMinSize request with *size* elements (0, 1, 2, ...) and a 4-byte LF."""
    return uINT8ArrayArray(4, bytes(range(size))).serialize()


def array_payload_with_declared_length(data: bytes, declared_length: int, length_field_length: int = 4) -> bytes:
    """Build an echoUINT8Array payload with the length field set to *declared_length*, not len(*data*).

    All deployed methods use a 4-byte length field. Thus the default *length_field_length* is 4.
    """
    length_fmt = {1: ">B", 2: ">H", 4: ">L"}[length_field_length]
    return struct.pack(length_fmt, declared_length) + data


def bitfields_request(b8: int, b16: int, b32: int) -> bytes:
    """Pack the echoBitfields request payload (big-endian u8, u16, u32)."""
    return struct.pack(">BHI", b8, b16, b32)


def bitfields_response(payload: bytes) -> tuple[int, int, int]:
    """Unpack the echoBitfields response into (bitfield8_out, bitfield16_out, bitfield32_out)."""
    return struct.unpack(">BHI", payload[:7])


def byte_order_request(u8: int, u16: int) -> bytes:
    """Pack the echoByteOrder request: a uint8, then a big-endian uint16."""
    return struct.pack(">BH", u8, u16)


def byte_order_response(payload: bytes) -> int:
    """Return the big-endian uint32 of the echoByteOrder response."""
    (value,) = struct.unpack(">I", payload[:4])
    return value


def common_datatypes_request(b, u8, u16, u32, i8, i16, i32, f) -> bytes:
    """Pack the echoCommonDatatypes IN payload: bool, u8, u16, u32, i8, i16, i32, float."""
    return struct.pack(_COMMON_DATATYPES_IN_FMT, b, u8, u16, u32, i8, i16, i32, f)


def float32_eq(a: float, b: float) -> bool:
    """Compare two IEEE-754 single values bit by bit. The byte comparison also works for NaN."""
    return struct.pack(">f", a) == struct.pack(">f", b)


def float64_eq(a: float, b: float) -> bool:
    """Compare two IEEE-754 double values bit by bit. The byte comparison also works for NaN."""
    return struct.pack(">d", a) == struct.pack(">d", b)


def utf16_text(s: str) -> bytes:
    """Encode *s* as UTF-16 big endian, without a BOM."""
    return s.encode("utf-16-be")


def dynamic_payload(bom: bytes, text: bytes, declared_length: int | None = None) -> bytes:
    """Pack a dynamic string: a uint32 length, then *bom* and *text*. *declared_length* replaces the true length."""
    body = bom + text
    declared = len(body) if declared_length is None else declared_length
    return struct.pack(">L", declared) + body


def fixed_body(bom: bytes, text: bytes) -> bytes:
    """Pad or cut *bom* + *text* to exactly 64 bytes, as a valid FIXED request."""
    body = bom + text
    return (body + b"\x00" * 64)[:64]


def union_payload(type_tag: int, value: bytes, declared_length: int | None = None) -> bytes:
    """Build a raw echoUNION request: len (u32), type (u32), and the value bytes.

    *declared_length* replaces the length field and is independent of len(value). Use it to test a length mismatch.
    """
    body = struct.pack(">L", type_tag) + value
    declared = len(body) if declared_length is None else declared_length
    return struct.pack(">L", declared) + body


def trigger_request(start: int, duration: int, debounce: int) -> bytes:
    """Pack the triggerEvent request: start value, duration, and debounce, one byte each."""
    return bytes([start, duration, debounce])
