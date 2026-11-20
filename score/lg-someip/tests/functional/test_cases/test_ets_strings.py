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

"""String tests for echoUTF16DYNAMIC/FIXED and echoUTF8DYNAMIC/FIXED.

Wire format:
  DYNAMIC (methods 68, 70): len(u32 BE) + BOM + encoded text, as one byte blob
    with a length prefix. echoUINT8Array uses the same array deserialization.
  FIXED (methods 69, 71): exactly 64 raw bytes (BOM + text, zero-padded), no length
    field. echoStaticUINT8Array uses the same fixed-array deserialization.

BOM: UTF-16 big-endian FE FF; UTF-8 EF BB BF.
"""

from __future__ import annotations

import asyncio

import pytest
from attribute_plugin import add_test_properties
from someip_itf.ets.calls import assert_dut_alive_after_timeout
from someip_itf.ets.client import MessageType, ReturnCode
from someip_itf.ets.constants import (
    METHOD_ECHO_UTF8_DYNAMIC,
    METHOD_ECHO_UTF8_FIXED,
    METHOD_ECHO_UTF16_DYNAMIC,
    METHOD_ECHO_UTF16_FIXED,
    call,
)
from someip_itf.ets.payload_types import Echo64ByteFixedArrayType as FixedPayload
from someip_itf.ets.payload_types import uINT8ArrayArray as ArrayPayload
from someip_itf.ets.payloads import dynamic_payload, fixed_body, utf16_text

pytestmark = pytest.mark.asyncio(loop_scope="class")

BOM_UTF16 = b"\xfe\xff"
BOM_UTF8 = b"\xef\xbb\xbf"
BOM_UTF16_WRONG = b"\xff\xfe"  # little-endian BOM, not correct for big-endian data
BOM_UTF8_WRONG = b"\xef\xbb\xbe"  # one byte is different from the real UTF-8 BOM


class TestUtf16Dynamic:
    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_039"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_ets_039_utf16_dynamic_round_trip(self, ets_client):
        """The server returns a dynamic UTF-16 string with its BOM unchanged."""
        text = utf16_text("HHHH")
        payload = dynamic_payload(BOM_UTF16, text)
        result = await call(ets_client, METHOD_ECHO_UTF16_DYNAMIC, payload)
        assert result.message_type == MessageType.RESPONSE
        assert result.return_code == ReturnCode.E_OK
        assert ArrayPayload(4).deserialize(result.payload).values == BOM_UTF16 + text

    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_040"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_ets_040_utf16_dynamic_length_too_long(self, ets_client):
        """The server rejects or ignores a declared length longer than the UTF-16 string.

        The test also passes if the DUT sends no reply. The test case requires an E_MALFORMED_MESSAGE reply.
        """
        payload = dynamic_payload(BOM_UTF16, utf16_text("H"), declared_length=200)
        try:
            result = await call(ets_client, METHOD_ECHO_UTF16_DYNAMIC, payload)
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
            "The byte-blob deserialization cuts the data to the declared length and "
            "returns E_OK. It does not detect that the result is only a BOM without text, "
            "and it does not reject it with E_MALFORMED_MESSAGE."
        ),
    )
    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_041"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_ets_041_utf16_dynamic_truncated_to_incomplete_bom(self, ets_client):
        """The server rejects as malformed a declared length that cuts a UTF-16 string to its BOM.

        The test does not send the full-length request and its echo first.
        echoUTF16DYNAMIC uses the echoUINT8Array deserialization. It cuts the data to the declared length.
        It answers E_OK and does not detect that the cut string is not valid.
        """
        text = utf16_text("H" * 63)
        payload = dynamic_payload(BOM_UTF16, text, declared_length=2)
        result = await call(ets_client, METHOD_ECHO_UTF16_DYNAMIC, payload)
        assert result.message_type == MessageType.ERROR
        assert result.return_code == ReturnCode.E_MALFORMED_MESSAGE

    @pytest.mark.xfail(
        strict=True,
        raises=AssertionError,
        reason=(
            "The byte-blob deserialization cuts the data to the declared length and "
            "returns E_OK, not E_MALFORMED_MESSAGE."
        ),
    )
    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_042"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_ets_042_utf16_dynamic_length_too_short(self, ets_client):
        """The server rejects as malformed a declared length shorter than the UTF-16 string.

        The root cause is the same as for the incomplete BOM test: the server cuts the data and answers E_OK.
        """
        text = utf16_text("HH")
        payload = dynamic_payload(BOM_UTF16, text, declared_length=len(BOM_UTF16) + 1)
        result = await call(ets_client, METHOD_ECHO_UTF16_DYNAMIC, payload)
        assert result.message_type == MessageType.ERROR
        assert result.return_code == ReturnCode.E_MALFORMED_MESSAGE

    @pytest.mark.xfail(
        strict=True,
        raises=AssertionError,
        reason=(
            "echoUTF16DYNAMIC does not check the UTF-16 code-unit alignment. "
            "It returns a string with an odd byte before its 0x0000 terminator with E_OK. "
            "It does not reject it with E_MALFORMED_MESSAGE."
        ),
    )
    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_043"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_ets_043_utf16_dynamic_odd_length_before_termination(self, ets_client):
        """The server rejects as malformed a UTF-16 string with an odd byte before its terminator.

        The declared length includes the BOM, the text, the odd byte and the 0x0000 terminator.
        echoUTF16DYNAMIC does not check the UTF-16 code-unit alignment. Thus it returns the payload unchanged.
        """
        text = utf16_text("HH") + b"\x48" + b"\x00\x00"  # the odd byte breaks the 16-bit alignment
        payload = dynamic_payload(BOM_UTF16, text)
        result = await call(ets_client, METHOD_ECHO_UTF16_DYNAMIC, payload)
        assert result.message_type == MessageType.ERROR
        assert result.return_code == ReturnCode.E_MALFORMED_MESSAGE

    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_044", "SOMEIP_ETS_003"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_ets_044_utf16_dynamic_odd_byte_after_termination_stripped(self, ets_client):
        """The server removes an extra byte after a correct UTF-16 string from the response.

        The string has no 0x0000 terminator. The extra byte is after the declared length.
        Thus the declared length is shorter than the bytes sent.
        """
        text = utf16_text("HH")
        well_formed = BOM_UTF16 + text
        payload = dynamic_payload(BOM_UTF16, text + b"\xff", declared_length=len(well_formed))
        result = await call(ets_client, METHOD_ECHO_UTF16_DYNAMIC, payload)
        assert result.message_type == MessageType.RESPONSE
        assert result.return_code == ReturnCode.E_OK
        assert ArrayPayload(4).deserialize(result.payload).values == well_formed

    @pytest.mark.xfail(
        strict=True,
        raises=AssertionError,
        reason="echoUTF16DYNAMIC does not check the BOM. It returns a BOM with the wrong endianness with E_OK.",
    )
    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_045"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_ets_045_utf16_dynamic_wrong_bom(self, ets_client):
        """The server rejects as malformed a UTF-16 string with a BOM that is not big-endian.

        echoUTF16DYNAMIC does not check the BOM.
        """
        payload = dynamic_payload(BOM_UTF16_WRONG, utf16_text("H"))
        result = await call(ets_client, METHOD_ECHO_UTF16_DYNAMIC, payload)
        assert result.message_type == MessageType.ERROR
        assert result.return_code == ReturnCode.E_MALFORMED_MESSAGE


class TestUtf16Fixed:
    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_046"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_ets_046_utf16_fixed_round_trip(self, ets_client):
        """The server returns a fixed 64-byte UTF-16 string unchanged."""
        body = fixed_body(BOM_UTF16, utf16_text("H" * 30))
        payload = FixedPayload(values=body).serialize()
        result = await call(ets_client, METHOD_ECHO_UTF16_FIXED, payload)
        assert result.message_type == MessageType.RESPONSE
        assert result.return_code == ReturnCode.E_OK
        assert FixedPayload().deserialize(result.payload).values == body

    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_047"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_ets_047_utf16_fixed_odd_byte_after_termination_stripped(self, ets_client):
        """The server removes an extra byte after a fixed 64-byte UTF-16 string from the response.

        echoUTF16FIXED accepts payloads that are too long, as echoStaticUINT8Array does.
        """
        body = fixed_body(BOM_UTF16, utf16_text("H" * 30))
        result = await call(ets_client, METHOD_ECHO_UTF16_FIXED, body + b"\xff")
        assert result.message_type == MessageType.RESPONSE
        assert result.return_code == ReturnCode.E_OK
        assert FixedPayload().deserialize(result.payload).values == body

    @pytest.mark.xfail(
        strict=True,
        raises=AssertionError,
        reason=(
            "echoUTF16FIXED uses the fixed-array deserialization of echoStaticUINT8Array. "
            "It accepts a payload that is too long, cuts it and returns E_OK. "
            "It does not reject it with E_MALFORMED_MESSAGE."
        ),
    )
    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_063"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_ets_063_utf16_fixed_too_long(self, ets_client):
        """The server rejects a fixed UTF-16 string longer than 64 bytes. It does not cut it.

        echoUTF16FIXED uses the fixed-array deserialization of echoStaticUINT8Array. It cuts the data and answers E_OK.
        """
        too_long = BOM_UTF16 + utf16_text("H" * 40)  # 82 bytes, no terminator
        result = await call(ets_client, METHOD_ECHO_UTF16_FIXED, too_long)
        assert result.message_type == MessageType.ERROR
        assert result.return_code == ReturnCode.E_MALFORMED_MESSAGE

    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_064"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_ets_064_utf16_fixed_too_short(self, ets_client):
        """The server rejects as malformed a fixed UTF-16 string shorter than 64 bytes.

        echoUTF16FIXED rejects payloads that are too short, as echoStaticUINT8Array does.
        """
        body = fixed_body(BOM_UTF16, utf16_text("H"))[:60]
        result = await call(ets_client, METHOD_ECHO_UTF16_FIXED, body)
        assert result.message_type == MessageType.ERROR
        assert result.return_code == ReturnCode.E_MALFORMED_MESSAGE


class TestUtf8Dynamic:
    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_048"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_ets_048_utf8_dynamic_round_trip(self, ets_client):
        """The server returns a dynamic UTF-8 string with its BOM unchanged."""
        text = "HHHH".encode()
        payload = dynamic_payload(BOM_UTF8, text)
        result = await call(ets_client, METHOD_ECHO_UTF8_DYNAMIC, payload)
        assert result.message_type == MessageType.RESPONSE
        assert result.return_code == ReturnCode.E_OK
        assert ArrayPayload(4).deserialize(result.payload).values == BOM_UTF8 + text

    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_049"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_ets_049_utf8_dynamic_length_too_long(self, ets_client):
        """The server rejects or ignores a declared length longer than the UTF-8 string.

        The test also passes if the DUT sends no reply. The test case requires an E_MALFORMED_MESSAGE reply.
        """
        payload = dynamic_payload(BOM_UTF8, b"H", declared_length=200)
        try:
            result = await call(ets_client, METHOD_ECHO_UTF8_DYNAMIC, payload)
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
            "The byte-blob deserialization cuts the data to the declared length and "
            "returns E_OK, not E_MALFORMED_MESSAGE."
        ),
    )
    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_050"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_ets_050_utf8_dynamic_truncated_to_incomplete_bom(self, ets_client):
        """The server rejects as malformed a declared length that cuts a UTF-8 string inside its BOM.

        The test does not send the full-length request and its echo first.
        The root cause is the same as for the UTF-16 incomplete BOM test: the server cuts the data and answers E_OK.
        """
        text = b"H" * 60
        payload = dynamic_payload(BOM_UTF8, text, declared_length=2)
        result = await call(ets_client, METHOD_ECHO_UTF8_DYNAMIC, payload)
        assert result.message_type == MessageType.ERROR
        assert result.return_code == ReturnCode.E_MALFORMED_MESSAGE

    @pytest.mark.xfail(
        strict=True,
        raises=AssertionError,
        reason=(
            "The byte-blob deserialization cuts the data to the declared length and "
            "returns E_OK, not E_MALFORMED_MESSAGE."
        ),
    )
    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_051"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_ets_051_utf8_dynamic_length_too_short(self, ets_client):
        """The server rejects as malformed a declared length shorter than the UTF-8 string."""
        text = b"HH"
        payload = dynamic_payload(BOM_UTF8, text, declared_length=len(BOM_UTF8) + 1)
        result = await call(ets_client, METHOD_ECHO_UTF8_DYNAMIC, payload)
        assert result.message_type == MessageType.ERROR
        assert result.return_code == ReturnCode.E_MALFORMED_MESSAGE

    @pytest.mark.xfail(
        strict=True,
        raises=AssertionError,
        reason="echoUTF8DYNAMIC does not check the BOM. It returns a wrong BOM with E_OK.",
    )
    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_052"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_ets_052_utf8_dynamic_wrong_bom(self, ets_client):
        """The server rejects as malformed a UTF-8 string with a BOM that is not the 3-byte UTF-8 marker."""
        payload = dynamic_payload(BOM_UTF8_WRONG, b"H")
        result = await call(ets_client, METHOD_ECHO_UTF8_DYNAMIC, payload)
        assert result.message_type == MessageType.ERROR
        assert result.return_code == ReturnCode.E_MALFORMED_MESSAGE


class TestUtf8Fixed:
    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_053"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_ets_053_utf8_fixed_round_trip(self, ets_client):
        """The server returns a fixed 64-byte UTF-8 string unchanged."""
        body = fixed_body(BOM_UTF8, b"H" * 30)
        payload = FixedPayload(values=body).serialize()
        result = await call(ets_client, METHOD_ECHO_UTF8_FIXED, payload)
        assert result.message_type == MessageType.RESPONSE
        assert result.return_code == ReturnCode.E_OK
        assert FixedPayload().deserialize(result.payload).values == body

    @pytest.mark.xfail(
        strict=True,
        raises=AssertionError,
        reason=(
            "echoUTF8FIXED uses the fixed-array deserialization of echoStaticUINT8Array. "
            "It accepts a payload that is too long, cuts it and returns E_OK. "
            "It does not reject it with E_MALFORMED_MESSAGE."
        ),
    )
    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_065"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_ets_065_utf8_fixed_too_long(self, ets_client):
        """The server rejects a fixed UTF-8 string longer than 64 bytes. It does not cut it.

        The root cause is the same as for UTF-16: the fixed-array deserialization cuts payloads that are too long.
        """
        too_long = BOM_UTF8 + b"H" * 80  # 83 bytes, no terminator
        result = await call(ets_client, METHOD_ECHO_UTF8_FIXED, too_long)
        assert result.message_type == MessageType.ERROR
        assert result.return_code == ReturnCode.E_MALFORMED_MESSAGE

    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_066"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_ets_066_utf8_fixed_too_short(self, ets_client):
        """The server rejects as malformed a fixed UTF-8 string shorter than 64 bytes.

        echoUTF8FIXED rejects payloads that are too short, as echoStaticUINT8Array does.
        """
        body = fixed_body(BOM_UTF8, b"H")[:60]
        result = await call(ets_client, METHOD_ECHO_UTF8_FIXED, body)
        assert result.message_type == MessageType.ERROR
        assert result.return_code == ReturnCode.E_MALFORMED_MESSAGE
