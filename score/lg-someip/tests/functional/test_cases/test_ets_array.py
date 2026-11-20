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

"""Array serialization tests for the ETS SOME/IP service.

The round-trip and oversize tests use six array methods:

  echoUINT8Array           (id=9)  - dynamic, default 4-byte length field
  echoUINT8Array8BitLength (id=62) - named 8-bit LF; the ARXML deploys the 4-byte default
  echoUINT8Array16BitLength(id=63) - named 16-bit LF; the ARXML deploys the 4-byte default
  echoStaticUINT8Array     (id=54) - fixed N=5, no length field on the wire
  echoUINT8ArrayMinSize    (id=55) - dynamic, ARXML max=5, the server does not check the min
  echoUINT8Array2Dim       (id=53) - 2-D nested dynamic arrays, outer and inner max=5

The ARXML has no SIZE-OF-ARRAY-LENGTH-FIELD for the 1-D dynamic-array methods.
Thus the 4-byte default applies, also to the 8-bit and 16-bit named variants.
A 1-byte or 2-byte length field is shorter than the deployed 4-byte length field.
The server sees a truncated payload and rejects it with ERROR/E_MALFORMED_MESSAGE.

The server handlers return the data unchanged. The SOME/IP skeleton checks the
payload length before it calls the handler. It answers truncated requests with
E_MALFORMED_MESSAGE. It accepts extra bytes after the expected arguments.
"""

from __future__ import annotations

import asyncio
import socket
import struct

import pytest
from attribute_plugin import add_test_properties
from someip_itf.ets.client import MessageType, ReturnCode
from someip_itf.ets.constants import (
    ARRAY_MAX_SIZE,
    ETS_SERVER_UDP_PORT,
    METHOD_ECHO_STATIC_UINT8_ARRAY,
    METHOD_ECHO_UINT8,
    METHOD_ECHO_UINT8_ARRAY,
    METHOD_ECHO_UINT8_ARRAY_2DIM,
    METHOD_ECHO_UINT8_ARRAY_8BIT_LENGTH,
    METHOD_ECHO_UINT8_ARRAY_16BIT_LENGTH,
    METHOD_ECHO_UINT8_ARRAY_MIN_SIZE,
    STATIC_ARRAY_SIZE,
    call,
)
from someip_itf.ets.payload_types import (
    EtsStaticUINT8ArrayInUInt8ArrayType as StaticArrayPayload,
)
from someip_itf.ets.payload_types import (
    EtsUINT8Array2DimInuINT8ArrayArrayArrayType as Array2DPayload,
)
from someip_itf.ets.payload_types import (
    uINT8ArrayArray as ArrayPayload,
)
from someip_itf.ets.payloads import array_payload_with_declared_length, minsize_payload

pytestmark = pytest.mark.asyncio(loop_scope="class")


class TestEtsArrayMarshalling:
    @pytest.mark.parametrize(
        "method_id",
        [
            METHOD_ECHO_UINT8_ARRAY,
            METHOD_ECHO_UINT8_ARRAY_8BIT_LENGTH,
            METHOD_ECHO_UINT8_ARRAY_16BIT_LENGTH,
        ],
        ids=["default", "8bit-named", "16bit-named"],
    )
    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_028"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_dynamic_array_empty(self, ets_client, method_id):
        """The three dynamic-length methods return an empty array unchanged.

        The request uses the deployed 4-byte length field, also for the 8-bit and 16-bit length methods.
        Thus only the echoUINT8Array parameter is related to the test case.
        """
        payload = ArrayPayload(4, b"").serialize()
        result = await call(ets_client, method_id, payload)
        assert result.message_type == MessageType.RESPONSE
        assert result.return_code == ReturnCode.E_OK
        assert ArrayPayload(4).deserialize(result.payload).values == b""

    @pytest.mark.parametrize(
        "method_id",
        [
            METHOD_ECHO_UINT8_ARRAY,
            METHOD_ECHO_UINT8_ARRAY_8BIT_LENGTH,
            METHOD_ECHO_UINT8_ARRAY_16BIT_LENGTH,
        ],
        ids=["default", "8bit-named", "16bit-named"],
    )
    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_028"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_dynamic_array_single(self, ets_client, method_id):
        """The three dynamic-length methods return a single-element array unchanged.

        The request uses the deployed 4-byte length field, also for the 8-bit and 16-bit length methods.
        Thus only the echoUINT8Array parameter is related to the test case.
        """
        data = b"\xab"
        payload = ArrayPayload(4, data).serialize()
        result = await call(ets_client, method_id, payload)
        assert result.message_type == MessageType.RESPONSE
        assert result.return_code == ReturnCode.E_OK
        assert ArrayPayload(4).deserialize(result.payload).values == data

    @pytest.mark.parametrize("size", [0, 1, ARRAY_MAX_SIZE])
    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_031"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_8bit_length_with_deployment_4byte(self, ets_client, size):
        """The 8-bit length method returns arrays with the deployed 4-byte length field unchanged.

        The test case expects a 1-byte length field. test_ets_031_8bit_length_field_round_trip checks it.
        """
        data = bytes(range(size))
        payload = ArrayPayload(4, data).serialize()
        result = await call(ets_client, METHOD_ECHO_UINT8_ARRAY_8BIT_LENGTH, payload)
        assert result.message_type == MessageType.RESPONSE
        assert result.return_code == ReturnCode.E_OK
        assert ArrayPayload(4).deserialize(result.payload).values == data

    @pytest.mark.parametrize("size", [1, 254, 255])
    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_031"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_8bit_length_named_size(self, ets_client, size):
        """A request to the 8-bit length method with a 1-byte length field gets ERROR/E_MALFORMED_MESSAGE.

        The 1-byte length field is shorter than the deployed 4-byte length field. Thus the payload is truncated.
        A truncated payload gets E_MALFORMED_MESSAGE.
        """
        data = bytes(range(size))
        payload = ArrayPayload(1, data).serialize()
        result = await call(ets_client, METHOD_ECHO_UINT8_ARRAY_8BIT_LENGTH, payload)
        assert result.message_type == MessageType.ERROR
        assert result.return_code == ReturnCode.E_MALFORMED_MESSAGE

    @pytest.mark.xfail(
        strict=True,
        raises=AssertionError,
        reason="echoUINT8Array8BitLength has a deployed 4-byte length field. A 1-byte "
        "length field is too short and gets E_MALFORMED_MESSAGE",
    )
    @pytest.mark.parametrize("size", [0, 1, ARRAY_MAX_SIZE, 255])
    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_031"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_ets_031_8bit_length_field_round_trip(self, ets_client, size):
        """echoUINT8Array8BitLength returns an array with a 1-byte length field unchanged.

        test_8bit_length_named_size checks the current DUT behavior. The DUT answers E_MALFORMED_MESSAGE.
        """
        payload = ArrayPayload(1, bytes(range(size))).serialize()
        result = await call(ets_client, METHOD_ECHO_UINT8_ARRAY_8BIT_LENGTH, payload)
        assert result.message_type == MessageType.RESPONSE
        assert result.return_code == ReturnCode.E_OK
        assert result.payload == payload

    @pytest.mark.parametrize("size", [0, ARRAY_MAX_SIZE])
    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_029"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_16bit_length_with_deployment_4byte(self, ets_client, size):
        """The 16-bit length method returns arrays with the deployed 4-byte length field unchanged.

        The test case expects a 2-byte length field. test_ets_029_16bit_length_field_round_trip checks it.
        """
        data = bytes(range(size))
        payload = ArrayPayload(4, data).serialize()
        result = await call(ets_client, METHOD_ECHO_UINT8_ARRAY_16BIT_LENGTH, payload)
        assert result.message_type == MessageType.RESPONSE
        assert result.return_code == ReturnCode.E_OK
        assert ArrayPayload(4).deserialize(result.payload).values == data

    @pytest.mark.parametrize("size", [ARRAY_MAX_SIZE])
    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_029"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_16bit_length_named_size(self, ets_client, size):
        """A request to the 16-bit length method with a 2-byte length field gets ERROR/E_MALFORMED_MESSAGE.

        The 2-byte length field is shorter than the deployed 4-byte length field. Thus the payload is truncated.
        A truncated payload gets E_MALFORMED_MESSAGE.
        """
        data = bytes(range(size))
        payload = ArrayPayload(2, data).serialize()
        result = await call(ets_client, METHOD_ECHO_UINT8_ARRAY_16BIT_LENGTH, payload)
        assert result.message_type == MessageType.ERROR
        assert result.return_code == ReturnCode.E_MALFORMED_MESSAGE

    @pytest.mark.parametrize("size", [256, 1000])
    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_029"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_16bit_length_large_with_deployment_4byte(self, ets_client, size):
        """The 16-bit length method returns 256-element and 1000-element arrays with a 4-byte length field unchanged.

        The test case expects a 2-byte length field. test_ets_029_16bit_length_field_round_trip checks it.
        The test does not use a 1450-element array. Its 1470-byte message is too large for the 1416-byte UDP
        receive buffer of someipd.
        """
        data = bytes(i % 256 for i in range(size))
        payload = ArrayPayload(4, data).serialize()
        result = await call(ets_client, METHOD_ECHO_UINT8_ARRAY_16BIT_LENGTH, payload)
        assert result.message_type == MessageType.RESPONSE
        assert result.return_code == ReturnCode.E_OK
        assert ArrayPayload(4).deserialize(result.payload).values == data

    @pytest.mark.xfail(
        strict=True,
        raises=AssertionError,
        reason="echoUINT8Array16BitLength has a deployed 4-byte length field. A 2-byte "
        "length field is too short and gets E_MALFORMED_MESSAGE",
    )
    @pytest.mark.parametrize("size", [0, ARRAY_MAX_SIZE, 256, 1000])
    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_029"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_ets_029_16bit_length_field_round_trip(self, ets_client, size):
        """echoUINT8Array16BitLength returns an array with a 2-byte length field unchanged.

        test_16bit_length_named_size checks the current DUT behavior. The DUT answers E_MALFORMED_MESSAGE.
        """
        payload = ArrayPayload(2, bytes(i % 256 for i in range(size))).serialize()
        result = await call(ets_client, METHOD_ECHO_UINT8_ARRAY_16BIT_LENGTH, payload)
        assert result.message_type == MessageType.RESPONSE
        assert result.return_code == ReturnCode.E_OK
        assert result.payload == payload

    @pytest.mark.parametrize(
        "data",
        [
            bytes(STATIC_ARRAY_SIZE),
            bytes([0xFF] * STATIC_ARRAY_SIZE),
            bytes(range(1, STATIC_ARRAY_SIZE + 1)),
        ],
        ids=["zeros", "all-ff", "1-to-5"],
    )
    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_022"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_static_exact_size(self, ets_client, data):
        """The server returns a 5-element static array as raw bytes without a length field."""
        payload = StaticArrayPayload(values=data).serialize()
        result = await call(ets_client, METHOD_ECHO_STATIC_UINT8_ARRAY, payload)
        assert result.message_type == MessageType.RESPONSE
        assert result.return_code == ReturnCode.E_OK
        assert StaticArrayPayload().deserialize(result.payload).values == data

    # Static array too short: fewer than STATIC_ARRAY_SIZE bytes cannot fill the
    # fixed-size array. The server rejects the truncated payload with ERROR/E_MALFORMED_MESSAGE.
    @pytest.mark.parametrize("size", [0, 4])
    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_022"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_static_undersize(self, ets_client, size):
        payload = bytes(range(size))
        result = await call(ets_client, METHOD_ECHO_STATIC_UINT8_ARRAY, payload)
        assert result.message_type == MessageType.ERROR
        assert result.return_code == ReturnCode.E_MALFORMED_MESSAGE

    # Static array too long: the server accepts bytes after STATIC_ARRAY_SIZE
    # (non-strict policy). It returns exactly N bytes.
    @pytest.mark.parametrize("size", [6, 10])
    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_022"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_static_oversize(self, ets_client, size):
        payload = bytes(range(size))
        result = await call(ets_client, METHOD_ECHO_STATIC_UINT8_ARRAY, payload)
        assert result.message_type == MessageType.RESPONSE
        assert result.return_code == ReturnCode.E_OK
        assert len(result.payload) == STATIC_ARRAY_SIZE

    @pytest.mark.parametrize("size", [3, 4, ARRAY_MAX_SIZE])
    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_032"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_minsize_within_max(self, ets_client, size):
        """echoUINT8ArrayMinSize returns arrays within the ARXML max of 5 elements unchanged."""
        data = bytes(range(size))
        payload = ArrayPayload(4, data).serialize()
        result = await call(ets_client, METHOD_ECHO_UINT8_ARRAY_MIN_SIZE, payload)
        assert result.message_type == MessageType.RESPONSE
        assert result.return_code == ReturnCode.E_OK
        assert ArrayPayload(4).deserialize(result.payload).values == data

    @pytest.mark.parametrize("size", [0, 1, 2])
    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_032", "SOMEIP_ETS_033"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_minsize_violation(self, ets_client, size):
        """The server returns arrays shorter than the ARXML min of 3 elements with E_OK.

        This test checks the current DUT behavior: the server does not check MIN-NUMBER-OF-ELEMENTS.
        The test case requires E_MALFORMED_MESSAGE. test_ets_033_minsize_too_short_rejected checks it.
        """
        result = await call(ets_client, METHOD_ECHO_UINT8_ARRAY_MIN_SIZE, minsize_payload(size))
        assert result.message_type == MessageType.RESPONSE
        assert result.return_code == ReturnCode.E_OK
        assert ArrayPayload(4).deserialize(result.payload).values == bytes(range(size))

    @pytest.mark.xfail(
        strict=True,
        reason="The server does not check MIN-NUMBER-OF-ELEMENTS. It returns a too-short "
        "echoUINT8ArrayMinSize request with E_OK, not E_MALFORMED_MESSAGE",
    )
    @pytest.mark.parametrize("size", [0, 1, 2])
    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_033", "SOMEIP_ETS_032"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_ets_033_minsize_too_short_rejected(self, ets_client, size):
        """The server rejects an array shorter than the ARXML min of 3 elements with E_MALFORMED_MESSAGE.

        test_minsize_violation checks the current DUT behavior. The DUT returns such an array with E_OK.
        """
        result = await call(ets_client, METHOD_ECHO_UINT8_ARRAY_MIN_SIZE, minsize_payload(size))
        assert result.message_type == MessageType.ERROR
        assert result.return_code == ReturnCode.E_MALFORMED_MESSAGE

    # Oversize test: more than the ARXML max (5). The handler returns any length.
    @pytest.mark.parametrize("size", [6, 10, 16])
    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_032"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_minsize_oversize(self, ets_client, size):
        data = bytes(range(size))
        payload = ArrayPayload(4, data).serialize()
        result = await call(ets_client, METHOD_ECHO_UINT8_ARRAY_MIN_SIZE, payload)
        assert result.message_type == MessageType.RESPONSE
        assert result.return_code == ReturnCode.E_OK
        assert ArrayPayload(4).deserialize(result.payload).values == data

    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_030"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_2d_jagged(self, ets_client):
        """The server returns a jagged 2-D array within the outer and inner max of 5 unchanged."""
        rows = [b"\x01", b"\x02\x03", b"\x04\x05\x06"]
        payload = Array2DPayload(rows=rows).serialize()
        result = await call(ets_client, METHOD_ECHO_UINT8_ARRAY_2DIM, payload)
        assert result.message_type == MessageType.RESPONSE
        assert result.return_code == ReturnCode.E_OK
        assert Array2DPayload().deserialize(result.payload).rows == rows

    @pytest.mark.parametrize(
        "rows",
        [[], [b""], [b"", b"\x01"]],
        ids=["empty-outer", "empty-inner", "mixed"],
    )
    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_030"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_2d_empty_variants(self, ets_client, rows):
        """The server returns 2-D arrays with an empty outer array, an empty row or mixed rows unchanged."""
        payload = Array2DPayload(rows=rows).serialize()
        result = await call(ets_client, METHOD_ECHO_UINT8_ARRAY_2DIM, payload)
        assert result.message_type == MessageType.RESPONSE
        assert result.return_code == ReturnCode.E_OK
        assert Array2DPayload().deserialize(result.payload).rows == rows

    # Oversize test: the outer row count is more than the ARXML max (5). The handler does not check it.
    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_030"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_2d_oversize_outer(self, ets_client):
        rows = [bytes([i]) for i in range(6)]
        payload = Array2DPayload(rows=rows).serialize()
        result = await call(ets_client, METHOD_ECHO_UINT8_ARRAY_2DIM, payload)
        assert result.message_type == MessageType.RESPONSE
        assert result.return_code == ReturnCode.E_OK
        assert Array2DPayload().deserialize(result.payload).rows == rows

    # Oversize test: one inner row is longer than the ARXML inner max (5). The handler does not check it.
    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_030"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_2d_oversize_inner(self, ets_client):
        rows = [bytes(range(6))]
        payload = Array2DPayload(rows=rows).serialize()
        result = await call(ets_client, METHOD_ECHO_UINT8_ARRAY_2DIM, payload)
        assert result.message_type == MessageType.RESPONSE
        assert result.return_code == ReturnCode.E_OK
        assert Array2DPayload().deserialize(result.payload).rows == rows


# ---------------------------------------------------------------------------
# Declared array length is different from the actual payload length
# ---------------------------------------------------------------------------


class TestEtsArrayLengthMismatch:
    """Tests where the declared array length is different from the actual payload length."""

    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_001"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_ets_001_declared_length_exceeds_message(self, ets_client):
        """The server rejects or ignores a declared array length larger than the full message."""
        payload = array_payload_with_declared_length(b"\x11\x22", declared_length=0xFFFFFFF0)
        try:
            result = await call(ets_client, METHOD_ECHO_UINT8_ARRAY, payload)
        except asyncio.TimeoutError:
            return  # The DUT ignored the request, which this test accepts.
        assert result.message_type == MessageType.ERROR
        assert result.return_code == ReturnCode.E_MALFORMED_MESSAGE

    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_002"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_ets_002_declared_length_exceeds_actual_array(self, ets_client):
        """The server rejects or ignores a possible declared length that is longer than the array."""
        payload = array_payload_with_declared_length(b"\x11\x22", declared_length=3)
        try:
            result = await call(ets_client, METHOD_ECHO_UINT8_ARRAY, payload)
        except asyncio.TimeoutError:
            return  # The DUT ignored the request, which this test accepts.
        assert result.message_type == MessageType.ERROR
        assert result.return_code == ReturnCode.E_MALFORMED_MESSAGE

    @pytest.mark.parametrize(
        "declared_length,expected",
        [(1, b"\x11"), (0, b"")],
        ids=["ets_003-length_1", "ets_067-length_0"],
    )
    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_003", "SOMEIP_ETS_067"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_ets_003_067_declared_length_shorter_strips_payload(self, ets_client, declared_length, expected):
        """The server cuts the payload to a declared length that is shorter than the array.

        One case declares 1 byte and the other case declares 0 bytes.
        The pass criterion requires a normal response. The DUT must not ignore the request.
        """
        payload = array_payload_with_declared_length(b"\x11\x22\x33", declared_length=declared_length)
        result = await call(ets_client, METHOD_ECHO_UINT8_ARRAY, payload)
        assert result.message_type == MessageType.RESPONSE
        assert result.return_code == ReturnCode.E_OK
        assert ArrayPayload(4).deserialize(result.payload).values == expected, (
            f"Expected payload stripped to {declared_length} byte(s); got {result.payload!r}"
        )


class TestEtsMtuNear:
    """Echo near the MTU: the largest payload that fits in one UDP datagram without SOME/IP-TP."""

    async def test_mtu_near_payload_echo(self, ets_client):
        # someipd uses SOME/IP-TP segments when the UDP payload is more than 1412 bytes.
        # The TP overhead is 4 bytes (offset field). The maximum SOME/IP payload
        # in one datagram without TP is:
        #   1412 - 16 (SOME/IP header) = 1396 bytes
        # The deployment ARXML has a 4-byte global LENGTH-FIELD:
        #   1396 - 4 (LF) = 1392 bytes of array data.
        # The IP packet is 20 + 8 + 16 + 4 + 1392 = 1440 bytes. This is less than
        # the bridge MTU of 1500 bytes.
        size = 1392
        data = bytes(i % 256 for i in range(size))
        payload = ArrayPayload(4, data).serialize()  # 4-byte length field of the deployment
        result = await call(ets_client, METHOD_ECHO_UINT8_ARRAY_16BIT_LENGTH, payload)
        assert result.message_type == MessageType.RESPONSE
        assert result.return_code == ReturnCode.E_OK
        assert ArrayPayload(4).deserialize(result.payload).values == data, (
            f"payload mismatch on MTU-near echo ({size} bytes)"
        )


# ---------------------------------------------------------------------------
# Robustness: a malformed SOME/IP header must not crash the server
# ---------------------------------------------------------------------------


class TestEtsRobustness:
    """Robustness tests with malformed or truncated SOME/IP frames.

    The server must discard invalid frames and continue to answer valid requests.
    The tests send the frames with a raw socket, not with ScapySomeipClient.
    """

    async def test_malformed_header_server_resilient(self, ets_client, host_ip):
        """After a frame with protocol version 0xFF the server still answers a valid echoUINT8 request."""
        # Make a SOME/IP header with protocol-version=0xFF (not valid; valid = 0x01).
        # Message format: service_id(2) method_id(2) length(4) client_id(2) session_id(2)
        #                  proto_ver(1) iface_ver(1) msg_type(1) return_code(1)
        bad_frame = (
            struct.pack(
                ">HHIHHBBBB",
                0x0103,  # service_id (ETS)
                0x0008,  # method_id (echoUINT8)
                9,  # length = header_tail(8) + payload(1)
                0x0001,  # client_id
                0x0001,  # session_id
                0xFF,  # proto_version — intentionally invalid
                0x02,  # interface_version
                0x00,  # message_type REQUEST
                0x00,  # return_code E_OK
            )
            + b"\x42"
        )

        raw = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        try:
            raw.sendto(bad_frame, (host_ip, ETS_SERVER_UDP_PORT))
        finally:
            raw.close()

        # Give the server time to process or discard the bad frame.
        await asyncio.sleep(0.2)

        # The server must still answer a valid echoUINT8 request.
        result = await call(ets_client, METHOD_ECHO_UINT8, b"\x42")
        assert result.message_type == MessageType.RESPONSE
        assert result.return_code == ReturnCode.E_OK
        assert result.payload[:1] == b"\x42", "Server did not respond to valid request after receiving malformed frame"
