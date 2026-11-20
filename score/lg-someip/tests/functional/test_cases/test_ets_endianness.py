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

"""Endianness conformance tests.

`checkByteOrder(summandUINT8, summandUINT16) -> sum` adds a u8 and a u16 on
the server and returns a u32. If the server decodes the u16 with the wrong byte order,
the sum is very different from the expected value. Thus a byte-swap bug is easy to see.

Wire format (SOME/IP big-endian):
  Request  payload: [u8 | u16 BE]  = 3 bytes
  Response payload: [u32 BE]       = 4 bytes
"""

from __future__ import annotations

import pytest
from attribute_plugin import add_test_properties
from someip_itf.ets.client import MessageType, ReturnCode
from someip_itf.ets.constants import METHOD_CHECK_BYTE_ORDER, call
from someip_itf.ets.payloads import byte_order_request, byte_order_response

pytestmark = pytest.mark.asyncio(loop_scope="class")


class TestEndianness:
    @add_test_properties(
        partially_verifies=["AS__PRS_SOMEIP_00065", "AS__PRS_SOMEIP_00369", "SOMEIP_ETS_005"],
        test_type="requirements-based",
        derivation_technique="boundary-values",
    )
    @pytest.mark.parametrize(
        "u8_in,u16_in,expected_sum",
        [
            pytest.param(0, 0, 0, id="both_zero"),
            pytest.param(1, 0, 1, id="u8_only"),
            pytest.param(0, 1, 1, id="u16_lsb_only"),
            pytest.param(1, 256, 257, id="low_values"),
            pytest.param(0, 0xFF00, 0xFF00, id="msb_high"),
            pytest.param(0, 0x00FF, 0x00FF, id="msb_low"),
            pytest.param(0xFF, 0xFFFF, 0x100FE, id="carry"),
        ],
    )
    async def test_byte_order_sum(self, ets_client, u8_in, u16_in, expected_sum):
        """checkByteOrder returns the correct sum of a u8 and a u16.

        The operand values show a byte-swap bug at different bit positions of the u16.
        """
        result = await call(ets_client, METHOD_CHECK_BYTE_ORDER, byte_order_request(u8_in, u16_in))
        assert result.message_type == MessageType.RESPONSE
        assert result.return_code == ReturnCode.E_OK
        assert len(result.payload) == 4
        actual = byte_order_response(result.payload)
        assert actual == expected_sum, (
            f"checkByteOrder({u8_in}, {u16_in:#06x}): expected {expected_sum:#010x}, got {actual:#010x}"
        )
