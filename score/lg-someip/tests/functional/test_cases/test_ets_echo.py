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

"""Data type echo tests for the ETS SOME/IP service.

Each test sends a value to an ETS echo method and makes sure that the
response holds the same value. The test sends SOME/IP over UDP to someipd.
someipd forwards the call to the ETS application.
"""

from __future__ import annotations

import math
import struct
import sys

import pytest
from attribute_plugin import add_test_properties
from someip_itf.ets.calls import assert_ok
from someip_itf.ets.constants import (
    METHOD_ECHO_COMMON_DATATYPES,
    METHOD_ECHO_ENUM,
    METHOD_ECHO_FLOAT64,
    METHOD_ECHO_INT8,
    METHOD_ECHO_STRUCT_SIMPLE,
    METHOD_ECHO_TYPEDEF,
    METHOD_ECHO_UINT8,
    METHOD_ECHO_UINT8_E2E,
    call,
)
from someip_itf.ets.payload_types import (
    EtsSTRUCTSimpleInStructureSimpleArrayType as StructSimpleArray,
)
from someip_itf.ets.payloads import common_datatypes_request, float32_eq, float64_eq

pytestmark = pytest.mark.asyncio(loop_scope="class")

# ---------------------------------------------------------------------------
# Wire format of the echoCommonDatatypes response (method 35).
# The OUT order is float, i32, i16, i8, u32, u16, u8, bool (19 bytes). It is the IN order reversed.
# ---------------------------------------------------------------------------
_OUT_FMT = ">flhbLHB?"  # 4+4+2+1+4+2+1+1 = 19 bytes
_OUT_SIZE = struct.calcsize(_OUT_FMT)

_STRUCT_SIMPLE_ARRAY_SIZE = len(StructSimpleArray())  # 2 × (Float32 + Uint8) = 10 bytes


class TestEtsEcho:
    @pytest.mark.parametrize("value", [0x00, 0x2A, 0xFF])
    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_027"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_echo_uint8_boundaries(self, ets_client, value):
        """echoUINT8 returns each uint8 boundary value unchanged."""
        payload = struct.pack(">B", value)
        result = await call(ets_client, METHOD_ECHO_UINT8, payload)
        assert_ok(result)
        assert result.payload[:1] == payload

    @pytest.mark.parametrize(
        "length,counter,data_id,crc,value",
        [
            (1, 7, 0x91111D9D, 0xDEADBEEF, 0x42),
            (0, 0, 0, 0, 0x00),
            (0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFF),
        ],
        ids=["mid-range", "all-zero", "all-max"],
    )
    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_034"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_ets_034_echo_uint8_e2e(self, ets_client, length, counter, data_id, crc, value):
        """echoUINT8E2E returns length, counter, dataID, CRC and value unchanged.

        The DUT does not validate or calculate the CRC or the counter.
        The deployment is responsible for E2E checks, if any.
        """
        payload = struct.pack(">LLLLB", length, counter, data_id, crc, value)
        result = await call(ets_client, METHOD_ECHO_UINT8_E2E, payload)
        assert_ok(result)
        assert result.payload[:17] == payload

    @pytest.mark.parametrize("value", [-128, 0, 127])
    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_021"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_echo_int8_boundaries(self, ets_client, value):
        """echoINT8 returns each signed int8 boundary value unchanged."""
        payload = struct.pack(">b", value)
        result = await call(ets_client, METHOD_ECHO_INT8, payload)
        assert_ok(result)
        assert result.payload[:1] == payload

    @pytest.mark.parametrize(
        "value",
        [
            0.0,
            -0.0,
            1.0,
            -1.0,
            float("nan"),
            float("inf"),
            float("-inf"),
            5e-324,
            sys.float_info.max,
            -sys.float_info.max,
            sys.float_info.min,
            math.pi,
        ],
        ids=["+0", "-0", "1.0", "-1.0", "nan", "+inf", "-inf", "denormal", "max", "-max", "min-normal", "pi"],
    )
    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_019"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_echo_float64_boundaries(self, ets_client, value):
        """echoFLOAT64 returns each boundary value and special value bit-exact."""
        payload = struct.pack(">d", value)
        result = await call(ets_client, METHOD_ECHO_FLOAT64, payload)
        assert_ok(result)
        (echoed,) = struct.unpack(">d", result.payload[:8])
        assert float64_eq(echoed, value)

    @pytest.mark.parametrize(
        "b,u8,u16,u32,i8,i16,i32,f",
        [
            (False, 0, 0, 0, 0, 0, 0, 0.0),
            (True, 255, 65535, 2**32 - 1, 127, 32767, 2**31 - 1, 1.0),
            (False, 0, 0, 0, -128, -32768, -(2**31), -1.0),
        ],
        ids=["all-zero", "all-max", "signed-min"],
    )
    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_008"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_echo_common_datatypes_boundaries(self, ets_client, b, u8, u16, u32, i8, i16, i32, f):
        """echoCommonDatatypes returns each field unchanged at its boundary values.

        The response has the fields in reverse order, as the ETS interface defines.
        """
        payload = common_datatypes_request(b, u8, u16, u32, i8, i16, i32, f)
        result = await call(ets_client, METHOD_ECHO_COMMON_DATATYPES, payload)
        assert_ok(result)
        assert len(result.payload) >= _OUT_SIZE
        f_out, i32_out, i16_out, i8_out, u32_out, u16_out, u8_out, b_out = struct.unpack(
            _OUT_FMT, result.payload[:_OUT_SIZE]
        )
        assert float32_eq(f_out, f)
        assert i32_out == i32
        assert i16_out == i16
        assert i8_out == i8
        assert u32_out == u32
        assert u16_out == u16
        assert u8_out == u8
        assert b_out == b

    @pytest.mark.parametrize(
        "f_val",
        [
            0.0,
            -0.0,
            float("nan"),
            float("inf"),
            float("-inf"),
            1.4e-45,  # smallest positive IEEE-754 single denormal
        ],
        ids=["+0", "-0", "nan", "+inf", "-inf", "denormal"],
    )
    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_008"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_echo_common_datatypes_float_specials(self, ets_client, f_val):
        """echoCommonDatatypes returns NaN, ±Inf, ±0 and denormal floats bit-exact.

        The test compares only the float32 field. It sends the other fields as zero and does not check them.
        """
        payload = common_datatypes_request(False, 0, 0, 0, 0, 0, 0, f_val)
        result = await call(ets_client, METHOD_ECHO_COMMON_DATATYPES, payload)
        assert_ok(result)
        assert len(result.payload) >= _OUT_SIZE
        (f_out,) = struct.unpack(">f", result.payload[:4])
        assert float32_eq(f_out, f_val)

    # echoTYPEDEF round trip. The typedef is an alias of uint8.
    @pytest.mark.parametrize("value", [0x00, 0x01, 0x42, 0xFF])
    async def test_echo_typedef(self, ets_client, value):
        payload = struct.pack(">B", value)
        result = await call(ets_client, METHOD_ECHO_TYPEDEF, payload)
        assert_ok(result)
        assert result.payload[:1] == payload

    @pytest.mark.parametrize("value", [1, 2, 3, 4, 5, 255])
    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_009"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_echo_enum_valid_values(self, ets_client, value):
        """echoENUM returns each value of the ARXML CompuMethod unchanged."""
        payload = struct.pack(">B", value)
        result = await call(ets_client, METHOD_ECHO_ENUM, payload)
        assert_ok(result)
        assert result.payload[:1] == payload

    @pytest.mark.parametrize("value", [0, 7])
    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_009"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_echo_enum_out_of_range(self, ets_client, value):
        """echoENUM returns values outside the ARXML CompuMethod unchanged.

        These values are not in the declared enumeration. The test case specification does not use them.
        """
        payload = struct.pack(">B", value)
        result = await call(ets_client, METHOD_ECHO_ENUM, payload)
        assert_ok(result)
        assert result.payload[:1] == payload

    # echoSTRUCTSimple round trip. The payload is a fixed array[2] of {float32, uint8}.
    @pytest.mark.parametrize(
        "elements",
        [
            [(0.0, 0), (0.0, 0)],
            [(1.0, 42), (1.0, 42)],
            [(3.14, 7), (-2.5, 250)],
        ],
        ids=["zero", "single-value", "populated"],
    )
    async def test_echo_struct_simple_round_trip(self, ets_client, elements):
        (f0, u0), (f1, u1) = elements
        payload = StructSimpleArray(elements=[(f0, u0), (f1, u1)]).serialize()
        result = await call(ets_client, METHOD_ECHO_STRUCT_SIMPLE, payload)
        assert_ok(result)
        assert len(result.payload) >= _STRUCT_SIMPLE_ARRAY_SIZE
        out = StructSimpleArray().deserialize(result.payload[:_STRUCT_SIMPLE_ARRAY_SIZE]).elements
        assert float32_eq(out[0][0], f0)
        assert out[0][1] == u0
        assert float32_eq(out[1][0], f1)
        assert out[1][1] == u1
