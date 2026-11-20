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

"""echoUNION round-trip and length-field conformance tests.

On the wire, echoUNION (method 67) has len(u32) + type(u32) + value (size per type tag)
as one dynamic byte blob. The DUT sends this blob back unchanged.

The tests send two framings.
In the blob framing, the length includes the type and the value.
In the SOME/IP union framing, the length includes only the value.
"""

from __future__ import annotations

import asyncio
import struct

import pytest
from attribute_plugin import add_test_properties
from someip_itf.ets.calls import assert_dut_alive_after_timeout
from someip_itf.ets.client import MessageType, ReturnCode
from someip_itf.ets.constants import METHOD_ECHO_UNION, call
from someip_itf.ets.payload_types import uINT8ArrayArray as ArrayPayload
from someip_itf.ets.payloads import union_payload

pytestmark = pytest.mark.asyncio(loop_scope="class")


_UNION_MEMBERS = pytest.mark.parametrize(
    "type_tag,value",
    [(4, struct.pack(">L", 0xDEADBEEF)), (2, b"\x00"), (8, struct.pack(">f", 3.5))],
    ids=["type4-u32", "type2-u8", "type8-f32"],
)


class TestEtsUnion:
    @_UNION_MEMBERS
    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_070", "SOMEIP_ETS_071", "SOMEIP_ETS_073"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_union_blob_round_trip(self, ets_client, type_tag, value):
        """echoUNION sends back a blob with a length prefix. The length includes the type field and the value.

        test_ets_038_union_round_trip checks the SOME/IP union framing.
        """
        payload = union_payload(type_tag, value)
        result = await call(ets_client, METHOD_ECHO_UNION, payload)
        assert result.message_type == MessageType.RESPONSE
        assert result.return_code == ReturnCode.E_OK
        assert ArrayPayload(4).deserialize(result.payload).values == struct.pack(">L", type_tag) + value

    @pytest.mark.xfail(
        strict=True,
        raises=AssertionError,
        reason=(
            "echoUNION is a byte blob and its length includes the type field. "
            "Thus the DUT cuts a union with SOME/IP union framing to its length"
        ),
    )
    @_UNION_MEMBERS
    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_038"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_ets_038_union_round_trip(self, ets_client, type_tag, value):
        """The length, type and value fields of a union come back unchanged.

        test_union_blob_round_trip checks the current DUT behavior, which sends back the blob framing.
        """
        payload = struct.pack(">LL", len(value), type_tag) + value
        result = await call(ets_client, METHOD_ECHO_UNION, payload)
        assert result.message_type == MessageType.RESPONSE
        assert result.return_code == ReturnCode.E_OK
        assert result.payload == payload

    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_070"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_ets_070_union_length_exceeds_message(self, ets_client):
        """The DUT rejects or ignores a union length that is larger than the whole message.

        Partial check: the test also accepts no reply. The test case requires E_MALFORMED_MESSAGE.
        """
        payload = union_payload(2, b"\x11", declared_length=0xFFFFFFF0)
        try:
            result = await call(ets_client, METHOD_ECHO_UNION, payload)
        except asyncio.TimeoutError:
            # The DUT ignored the request, which this test accepts only if the DUT is still alive.
            # A hung or crashed DUT would time out here too.
            await assert_dut_alive_after_timeout(ets_client)
            return
        assert result.message_type == MessageType.ERROR
        assert result.return_code == ReturnCode.E_MALFORMED_MESSAGE

    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_071"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_ets_071_union_length_too_long(self, ets_client):
        """The DUT rejects or ignores a plausible union length that is longer than the sent bytes.

        Partial check: the test also accepts no reply. The test case requires E_MALFORMED_MESSAGE.
        """
        payload = union_payload(2, b"\x11", declared_length=20)
        try:
            result = await call(ets_client, METHOD_ECHO_UNION, payload)
        except asyncio.TimeoutError:
            # The DUT ignored the request, which this test accepts only if the DUT is still alive.
            # A hung or crashed DUT would time out here too.
            await assert_dut_alive_after_timeout(ets_client)
            return
        assert result.message_type == MessageType.ERROR
        assert result.return_code == ReturnCode.E_MALFORMED_MESSAGE

    @pytest.mark.xfail(
        strict=True,
        raises=AssertionError,
        reason=(
            "echoUNION is a byte blob with a length prefix. The DUT cuts the request "
            "to the length field and answers E_OK instead of E_MALFORMED_MESSAGE"
        ),
    )
    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_072"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_ets_072_union_length_too_short(self, ets_client):
        """The DUT rejects a union length shorter than its member with E_MALFORMED_MESSAGE.

        The length field is 2 bytes for a 4-byte uint32 member (type 4), with SOME/IP union framing.
        echoUNION decodes a byte blob. It cuts the request to 2 bytes and answers E_OK.
        """
        payload = struct.pack(">LL", 2, 4) + struct.pack(">L", 0x11223344)
        result = await call(ets_client, METHOD_ECHO_UNION, payload)
        assert result.message_type == MessageType.ERROR
        assert result.return_code == ReturnCode.E_MALFORMED_MESSAGE

    @pytest.mark.xfail(
        strict=True,
        raises=AssertionError,
        reason=(
            "The test needs a union check that knows the type. The check finds a type/payload-size "
            "mismatch and corrects the padding. echoUNION does not do this check. "
            "It sends back the type field and the value unchanged."
        ),
    )
    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_073"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_ets_073_union_wrong_type_field(self, ets_client):
        """A union with a type field that does not agree with its value comes back with the correct type and padding.

        echoUNION does not decode the union bytes. It sends them back unchanged and does not correct them.
        """
        # type=2 (uint8, 1 byte), but the payload has 4 bytes. A real union implementation
        # corrects the type or the padding. A byte-blob echo sends it back unchanged.
        payload = union_payload(2, struct.pack(">L", 0x11223344))
        result = await call(ets_client, METHOD_ECHO_UNION, payload)
        assert result.message_type == MessageType.RESPONSE
        assert result.return_code == ReturnCode.E_OK
        echoed = ArrayPayload(4).deserialize(result.payload).values
        (echoed_type,) = struct.unpack(">L", echoed[:4])
        assert echoed_type != 2, "expected the DUT to correct the incorrect type field"
