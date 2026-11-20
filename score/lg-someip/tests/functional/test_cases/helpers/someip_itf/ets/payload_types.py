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

"""SOME/IP payload dataclasses for the data types of the ETS service model.

Each public name is the SHORT-NAME of the data type in the model.
Use an `as` alias at the import if the name is too long.

All arrays use a 4-byte length field. No data type has a different value.
_Uint8Vector has length_field_length, so that tests can send other length field sizes.
"""

import struct
from dataclasses import dataclass

from someip_itf.ets.serialization import (
    Float32,
    SomeIpDynamicSizeArray,
    SomeIpFixedSizeArray,
    SomeIpPayload,
    Uint8,
    Uint32,
)

# ---------------------------------------------------------------------------
# TYPE_REFERENCE aliases
# ---------------------------------------------------------------------------

Enumeration1 = Uint8  # underlying type uint8_t
Build = Uint8  # underlying type uint8_t
TestEventUINT8EvtType = Uint8
TestEventUINT8E2EEvtType = Uint8
TestEventUINT8MulticastEvtType = Uint8

# ---------------------------------------------------------------------------
# STRUCTUREs
# ---------------------------------------------------------------------------


@dataclass
class StructureSimple(SomeIpPayload):
    """{ float floatStructureSimple, uint8 uint8StructureSimple }."""

    floatStructureSimple: Float32
    uint8StructureSimple: Uint8

    def __init__(self, f: float = 0.0, u: int = 0):
        self.floatStructureSimple = Float32(f)
        self.uint8StructureSimple = Uint8(u)


@dataclass
class VersionType(SomeIpPayload):
    """{ uint8 majorVersion, uint32 minorVersion }."""

    majorVersion: Uint8
    minorVersion: Uint32

    def __init__(self, major: int = 0, minor: int = 0):
        self.majorVersion = Uint8(major)
        self.minorVersion = Uint32(minor)


# ---------------------------------------------------------------------------
# ARRAYs: fixed size, no length field on the wire
# ---------------------------------------------------------------------------


@dataclass
class EtsStaticUINT8ArrayInUInt8ArrayType(SomeIpPayload):
    """uint8[5]."""

    _SIZE = 5
    data: SomeIpFixedSizeArray

    def __init__(self, values: bytes = b""):
        self.data = SomeIpFixedSizeArray(Uint8, self._SIZE)
        for i, v in enumerate(values[: self._SIZE]):
            self.data.data[i] = Uint8(v)

    @property
    def values(self) -> bytes:
        return bytes(e.value for e in self.data.data)


class Echo64ByteFixedArrayType(EtsStaticUINT8ArrayInUInt8ArrayType):
    """uint8[64]. echoUTF16FIXED and echoUTF8FIXED use this wire shape (raw bytes, with BOM)."""

    _SIZE = 64


@dataclass
class EtsSTRUCTSimpleInStructureSimpleArrayType(SomeIpPayload):
    """StructureSimple[2]."""

    data: SomeIpFixedSizeArray

    def __init__(self, elements: list[tuple[float, int]] | None = None):
        self.data = SomeIpFixedSizeArray(StructureSimple, 2)
        for i, (f, u) in enumerate((elements or [])[:2]):
            self.data.data[i] = StructureSimple(f, u)

    @property
    def elements(self) -> list[tuple[float, int]]:
        return [(s.floatStructureSimple.value, s.uint8StructureSimple.value) for s in self.data.data]


# The OUT type has the same wire shape, so it is an alias
EtsSTRUCTSimpleOutStructureSimpleArrayType = EtsSTRUCTSimpleInStructureSimpleArrayType

# ---------------------------------------------------------------------------
# VECTORs: dynamic uint8 arrays with the same shape
# ---------------------------------------------------------------------------


@dataclass
class _Uint8Vector(SomeIpPayload):
    """Implementation for all uint8 VECTOR<5> types of the service model.

    The five SHORT-NAMEs below are functionally the same. Each is an alias.
    """

    data: SomeIpDynamicSizeArray

    def __init__(self, length_field_length: int = 4, values: bytes = b""):
        self.data = SomeIpDynamicSizeArray(Uint8)
        self.data.length_field_length = length_field_length
        self.data.data = [Uint8(v) for v in values]

    @property
    def values(self) -> bytes:
        return bytes(e.value for e in self.data.data)


EnhancedTestabilityServiceLow2TestEventUINT8ArrayUInt8ArrayType = _Uint8Vector
uINT8ArrayArray = _Uint8Vector
uINT8ArrayReturnValueArray = _Uint8Vector
EtsUINT8ArrayMinSizeInUInt8ArrayType = _Uint8Vector
TestEventUINT8ArrayEvtType = _Uint8Vector

# ---------------------------------------------------------------------------
# Nested VECTOR<VECTOR<uint8>>: rows of different length need a custom wire encoding
# ---------------------------------------------------------------------------


class _Uint8VectorOfVector:
    """VECTOR<VECTOR<uint8, 5>, 5>.

    Wire format: [outer-LF: total bytes of all inner blocks]
                 ([inner-LF: len(row)] [row bytes])*

    SomeIpDynamicSizeArray sets _single_element_length when you make it.
    Thus it cannot model inner rows of different length. This class encodes the wire format directly.
    """

    _LF_FMT = {1: ">B", 2: ">H", 4: ">L"}

    def __init__(
        self,
        outer_lf: int = 4,
        inner_lf: int = 4,
        rows: list[bytes] | None = None,
    ):
        self._outer_lf = outer_lf
        self._inner_lf = inner_lf
        self.rows: list[bytes] = list(rows or [])

    def serialize(self) -> bytes:
        inner_fmt = self._LF_FMT[self._inner_lf]
        outer_fmt = self._LF_FMT[self._outer_lf]
        inner_blocks = b"".join(struct.pack(inner_fmt, len(r)) + r for r in self.rows)
        return struct.pack(outer_fmt, len(inner_blocks)) + inner_blocks

    def deserialize(self, payload: bytes) -> "_Uint8VectorOfVector":
        outer_fmt = self._LF_FMT[self._outer_lf]
        inner_fmt = self._LF_FMT[self._inner_lf]
        (total,) = struct.unpack_from(outer_fmt, payload)
        blocks = payload[self._outer_lf : self._outer_lf + total]
        rows, pos = [], 0
        while pos < len(blocks):
            (n,) = struct.unpack_from(inner_fmt, blocks, pos)
            rows.append(bytes(blocks[pos + self._inner_lf : pos + self._inner_lf + n]))
            pos += self._inner_lf + n
        self.rows = rows
        return self


EtsUINT8Array2DimInuINT8ArrayArrayArrayType = _Uint8VectorOfVector
EtsUINT8Array2DimOutuINT8ArrayReturnValueArrayArrayType = _Uint8VectorOfVector
