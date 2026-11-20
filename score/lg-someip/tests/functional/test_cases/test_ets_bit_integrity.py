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

"""Bit-level integrity tests.

The pass criterion for echoBitfields is a reversed bit order in each field.

`echoBitfields(bitfield8_in, bitfield16_in, bitfield32_in)` reverses the bit
order in each field independently:

    for i in 0..width-1: out_bit[width-1-i] = in_bit[i]

Wire format (SOME/IP big-endian):
  Request  payload: [u8 | u16 BE | u32 BE]  = 7 bytes
  Response payload: [u8 | u16 BE | u32 BE]  = 7 bytes
"""

from __future__ import annotations

import pytest
from attribute_plugin import add_test_properties
from someip_itf.ets.calls import check_walking_one
from someip_itf.ets.client import MessageType, ReturnCode
from someip_itf.ets.constants import METHOD_ECHO_BITFIELDS, call
from someip_itf.ets.payloads import bitfields_request, bitfields_response

pytestmark = pytest.mark.asyncio(loop_scope="class")


class TestBitLevelIntegrity:
    @add_test_properties(
        partially_verifies=[
            "AS__PRS_SOMEIP_00300",
            "AS__PRS_SOMEIP_00065",
            "AS__PRS_SOMEIP_00191",
            "SOMEIP_ETS_007",
        ],
        test_type="requirements-based",
        derivation_technique="equivalence-classes",
    )
    @pytest.mark.parametrize("bit_pos", range(8), ids=[f"bit{b}" for b in range(8)])
    async def test_bitfield8_walking_one(self, ets_client, bit_pos):
        """A single set bit in the 8-bit field comes back at the mirrored position."""
        await check_walking_one(ets_client, 8, bit_pos)

    @add_test_properties(
        partially_verifies=[
            "AS__PRS_SOMEIP_00300",
            "AS__PRS_SOMEIP_00065",
            "AS__PRS_SOMEIP_00191",
            "SOMEIP_ETS_007",
        ],
        test_type="requirements-based",
        derivation_technique="equivalence-classes",
    )
    @pytest.mark.parametrize("bit_pos", range(16), ids=[f"bit{b}" for b in range(16)])
    async def test_bitfield16_walking_one(self, ets_client, bit_pos):
        """A single set bit in the 16-bit field comes back at the mirrored position."""
        await check_walking_one(ets_client, 16, bit_pos)

    @add_test_properties(
        partially_verifies=[
            "AS__PRS_SOMEIP_00300",
            "AS__PRS_SOMEIP_00065",
            "AS__PRS_SOMEIP_00191",
            "SOMEIP_ETS_007",
        ],
        test_type="requirements-based",
        derivation_technique="equivalence-classes",
    )
    @pytest.mark.parametrize("bit_pos", range(32), ids=[f"bit{b}" for b in range(32)])
    async def test_bitfield32_walking_one(self, ets_client, bit_pos):
        """A single set bit in the 32-bit field comes back at the mirrored position."""
        await check_walking_one(ets_client, 32, bit_pos)

    @add_test_properties(
        partially_verifies=[
            "AS__PRS_SOMEIP_00300",
            "AS__PRS_SOMEIP_00065",
            "AS__PRS_SOMEIP_00191",
            "SOMEIP_ETS_007",
        ],
        test_type="requirements-based",
        derivation_technique="boundary-values",
    )
    @pytest.mark.parametrize(
        "b8_in,b16_in,b32_in",
        [
            pytest.param(0x00, 0x0000, 0x00000000, id="all_zeros"),
            pytest.param(0xFF, 0xFFFF, 0xFFFFFFFF, id="all_ones"),
        ],
    )
    async def test_boundary_invariants(self, ets_client, b8_in, b16_in, b32_in):
        """After the bit reversal, all-zeros and all-ones fields come back unchanged."""
        result = await call(ets_client, METHOD_ECHO_BITFIELDS, bitfields_request(b8_in, b16_in, b32_in))
        assert result.message_type == MessageType.RESPONSE
        assert result.return_code == ReturnCode.E_OK
        assert len(result.payload) == 7
        b8_out, b16_out, b32_out = bitfields_response(result.payload)
        assert b8_out == b8_in, f"bitfield8: 0x{b8_in:02x} → expected 0x{b8_in:02x}, got 0x{b8_out:02x}"
        assert b16_out == b16_in, f"bitfield16: 0x{b16_in:04x} → expected 0x{b16_in:04x}, got 0x{b16_out:04x}"
        assert b32_out == b32_in, f"bitfield32: 0x{b32_in:08x} → expected 0x{b32_in:08x}, got 0x{b32_out:08x}"

    @add_test_properties(
        partially_verifies=[
            "AS__PRS_SOMEIP_00300",
            "AS__PRS_SOMEIP_00065",
            "AS__PRS_SOMEIP_00191",
            "SOMEIP_ETS_007",
        ],
        test_type="requirements-based",
        derivation_technique="equivalence-classes",
    )
    @pytest.mark.parametrize(
        "b8_in,b16_in,b32_in,b8_exp,b16_exp,b32_exp",
        [
            pytest.param(
                0xAA,
                0xAAAA,
                0xAAAAAAAA,
                0x55,
                0x5555,
                0x55555555,
                id="0xAA_to_0x55",
            ),
            pytest.param(
                0x55,
                0x5555,
                0x55555555,
                0xAA,
                0xAAAA,
                0xAAAAAAAA,
                id="0x55_to_0xAA",
            ),
        ],
    )
    async def test_alternating_bit_pattern(self, ets_client, b8_in, b16_in, b32_in, b8_exp, b16_exp, b32_exp):
        """Alternating 0xAA patterns come back as 0x55 in each field, and 0x55 comes back as 0xAA."""
        result = await call(ets_client, METHOD_ECHO_BITFIELDS, bitfields_request(b8_in, b16_in, b32_in))
        assert result.message_type == MessageType.RESPONSE
        assert result.return_code == ReturnCode.E_OK
        assert len(result.payload) == 7
        b8_out, b16_out, b32_out = bitfields_response(result.payload)
        assert b8_out == b8_exp, f"bitfield8: 0x{b8_in:02x} → expected 0x{b8_exp:02x}, got 0x{b8_out:02x}"
        assert b16_out == b16_exp, f"bitfield16: 0x{b16_in:04x} → expected 0x{b16_exp:04x}, got 0x{b16_out:04x}"
        assert b32_out == b32_exp, f"bitfield32: 0x{b32_in:08x} → expected 0x{b32_exp:08x}, got 0x{b32_out:08x}"
