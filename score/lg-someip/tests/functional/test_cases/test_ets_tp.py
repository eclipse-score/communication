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

"""SOME/IP-TP segmentation tests.

ScapySomeipClient does not support TP. Thus the tests make each frame with
struct.pack and send it on a raw UDP socket.

Wire format:
  bytes 0-15  standard SOME/IP header; message_type has bit 0x20 (TP_FLAG) set
  bytes 16-19 TP header: 28-bit byte offset (bits 31..4) | reserved(3) | more-segments(bit0)
  bytes 20+   segment payload (<=1392 bytes)

The server only receives TP. It drops each segment that is not valid and writes a
warning to the log. It does not send a NACK or an ERROR for a bad segment.
Thus each negative test checks "no response, server still alive", not a return code.

A positive test (2 segments) first shows that the frames are correct.
Other tests send segments that are too short, not aligned, too large, duplicated,
out of order, incomplete, with a wrong length field, or with an offset that is too large.
A message without its final segment, or with a gap, is never delivered.
The server does not remove it later, because its cleanup timer is not active.
"""

from __future__ import annotations

import asyncio
import itertools
import struct

import pytest
from someip_itf.ets.calls import assert_ok
from someip_itf.ets.client import MessageType, ReturnCode
from someip_itf.ets.constants import (
    ETS_MAJOR_VERSION,
    ETS_SERVER_UDP_PORT,
    ETS_SERVICE_ID,
    METHOD_ECHO_UINT8,
    METHOD_ECHO_UINT8_ARRAY,
    METHOD_ECHO_UINT8_ARRAY_16BIT_LENGTH,
    call,
)
from someip_itf.ets.payload_types import uINT8ArrayArray as ArrayPayload
from someip_itf.ets.raw import expect_no_response, expect_response, open_bound_socket
from someip_itf.ets.tp import TP_FLAG, TP_HEADER_SIZE, send_message, tp_header, tp_segment

_LGSOMEIP_5_XFAIL_REASON = "[lg-someip #5] the default lg-someip build has no SOME/IP-TP reassembly"

pytestmark = pytest.mark.asyncio(loop_scope="class")

# Maximum TP segment length of the server.
_TP_MAX_SEGMENT_SIZE = 1392
_SOMEIP_HEADER_SIZE = 16  # full SOME/IP header size
_MAX_MESSAGE_SIZE = 50000  # maximum reassembled message size of the server

_CLIENT_ID = 0x0001
_TP_TEST_PORT = 60022  # different from the ports of the other test modules
_NO_RESPONSE_TIMEOUT = 3.0

_session_ids = itertools.count(0x4000)


@pytest.fixture
def session_id() -> int:
    """A new 16-bit session id for each test.

    All tests in this module use the same TP reassembly bucket
    (service/method/client/iface_ver/msg_type are constant). A unique session id
    for each test makes sure that the server discards an incomplete entry of a
    previous test. Thus the old entry does not interfere.
    """
    return next(_session_ids)


class TestSomeipTpSegmentation:
    # Positive control: a valid 2-segment message must reassemble and
    # behave as a request without segments. This shows that the frames are
    # correct, so that the negative tests are reliable.
    @pytest.mark.xfail(strict=True, reason=_LGSOMEIP_5_XFAIL_REASON)
    async def test_multi_segment_reassembly_completes(self, ets_client, host_ip, session_id):
        data = bytes(range(16))
        content = ArrayPayload(4, data).serialize()  # 4-byte LF + 16 bytes = 20 bytes

        sock = open_bound_socket(_TP_TEST_PORT)
        loop = asyncio.get_running_loop()
        try:
            send_message(sock, host_ip, METHOD_ECHO_UINT8_ARRAY_16BIT_LENGTH, session_id, content)
            raw = await expect_response(loop, sock)
        finally:
            sock.close()

        assert len(raw) >= 16
        assert raw[14] == int(MessageType.RESPONSE)
        assert raw[15] == int(ReturnCode.E_OK)
        assert ArrayPayload(4).deserialize(raw[16:]).values == data

        probe = await call(ets_client, METHOD_ECHO_UINT8, b"\x42")
        assert_ok(probe)

    # A "segment" with a datagram shorter than 16 (header) + 4 (TP header) bytes
    # fails the first length check. The server drops it before the segment validation.
    async def test_segment_shorter_than_tp_header_dropped(self, ets_client, host_ip, session_id):
        body_len = 8 + 2
        header = struct.pack(
            ">HHIHHBBBB",
            ETS_SERVICE_ID,
            METHOD_ECHO_UINT8_ARRAY_16BIT_LENGTH,
            body_len,
            _CLIENT_ID,
            session_id,
            0x01,
            ETS_MAJOR_VERSION,
            int(MessageType.REQUEST) | TP_FLAG,
            0x00,
        )
        frame = header + b"\x00\x00"  # only 2 bytes, but the TP header needs 4 bytes

        sock = open_bound_socket(_TP_TEST_PORT)
        loop = asyncio.get_running_loop()
        try:
            sock.sendto(frame, (host_ip, ETS_SERVER_UDP_PORT))
            await expect_no_response(loop, sock)
        finally:
            sock.close()

        probe = await call(ets_client, METHOD_ECHO_UINT8, b"\x42")
        assert_ok(probe)

    # A segment that is not the last one (more_segments=1) must have a payload
    # length that is a multiple of 16. If not, the server drops it.
    async def test_non_final_segment_not_multiple_of_16_dropped(self, ets_client, host_ip, session_id):
        frame = tp_segment(
            METHOD_ECHO_UINT8_ARRAY_16BIT_LENGTH,
            session_id,
            byte_offset=0,
            segment_payload=bytes(10),
            more_segments=True,
        )

        sock = open_bound_socket(_TP_TEST_PORT)
        loop = asyncio.get_running_loop()
        try:
            sock.sendto(frame, (host_ip, ETS_SERVER_UDP_PORT))
            await expect_no_response(loop, sock)
        finally:
            sock.close()

        probe = await call(ets_client, METHOD_ECHO_UINT8, b"\x42")
        assert_ok(probe)

    # The server drops a segment with a payload of more than 1392 bytes.
    # The datagram still fits in the 1416-byte UDP receive buffer of the server.
    async def test_segment_exceeds_max_segment_size_dropped(self, ets_client, host_ip, session_id):
        frame = tp_segment(
            METHOD_ECHO_UINT8_ARRAY_16BIT_LENGTH,
            session_id,
            byte_offset=0,
            segment_payload=bytes(_TP_MAX_SEGMENT_SIZE + 1),
            more_segments=False,
        )

        sock = open_bound_socket(_TP_TEST_PORT)
        loop = asyncio.get_running_loop()
        try:
            sock.sendto(frame, (host_ip, ETS_SERVER_UDP_PORT))
            await expect_no_response(loop, sock)
        finally:
            sock.close()

        probe = await call(ets_client, METHOD_ECHO_UINT8, b"\x42")
        assert_ok(probe)

    # The test sends a segment again. Its offset is in an area that is already
    # reassembled. The server drops it and does not add it a second time.
    # The message must be correct when the real final segment arrives.
    @pytest.mark.xfail(strict=True, reason=_LGSOMEIP_5_XFAIL_REASON)
    async def test_duplicate_segment_does_not_corrupt_reassembly(self, ets_client, host_ip, session_id):
        data = bytes(range(16))
        content = ArrayPayload(4, data).serialize()
        first, second = content[:16], content[16:]

        sock = open_bound_socket(_TP_TEST_PORT)
        loop = asyncio.get_running_loop()
        try:
            method = METHOD_ECHO_UINT8_ARRAY_16BIT_LENGTH
            sock.sendto(tp_segment(method, session_id, 0, first, True), (host_ip, ETS_SERVER_UDP_PORT))
            # Send the same first segment again. Offset 0 is less than the expected offset (16).
            sock.sendto(tp_segment(method, session_id, 0, first, True), (host_ip, ETS_SERVER_UDP_PORT))
            sock.sendto(tp_segment(method, session_id, 16, second, False), (host_ip, ETS_SERVER_UDP_PORT))
            raw = await expect_response(loop, sock)
        finally:
            sock.close()

        assert raw[14] == int(MessageType.RESPONSE)
        assert raw[15] == int(ReturnCode.E_OK)
        assert ArrayPayload(4).deserialize(raw[16:]).values == data, (
            "duplicate segment corrupted the reassembled payload"
        )

        probe = await call(ets_client, METHOD_ECHO_UINT8, b"\x42")
        assert_ok(probe)

    # A new first segment (offset 0) with a different session for the same
    # service/method/client/iface_ver/msg_type bucket replaces the incomplete message.
    # The server does not merge or reject it. The full message must contain only
    # the data of the second session, and no bytes of the first session.
    @pytest.mark.xfail(strict=True, reason=_LGSOMEIP_5_XFAIL_REASON)
    async def test_session_switch_mid_reassembly_discards_partial_message(self, ets_client, host_ip, session_id):
        method = METHOD_ECHO_UINT8_ARRAY_16BIT_LENGTH
        abandoned_session = session_id
        winning_session = next(_session_ids)

        data_a = bytes([0xAA] * 16)  # abandoned
        data_b = bytes([0xBB] * 16)  # must be the only content in the response
        content_a = ArrayPayload(4, data_a).serialize()
        content_b = ArrayPayload(4, data_b).serialize()

        sock = open_bound_socket(_TP_TEST_PORT)
        loop = asyncio.get_running_loop()
        try:
            # First segment of session A. The test does not complete it.
            sock.sendto(
                tp_segment(method, abandoned_session, 0, content_a[:16], True),
                (host_ip, ETS_SERVER_UDP_PORT),
            )
            # First segment of session B for the *same* bucket. It discards the incomplete message of A.
            sock.sendto(
                tp_segment(method, winning_session, 0, content_b[:16], True),
                (host_ip, ETS_SERVER_UDP_PORT),
            )
            # The final segment of session B completes the message.
            sock.sendto(
                tp_segment(method, winning_session, 16, content_b[16:], False),
                (host_ip, ETS_SERVER_UDP_PORT),
            )
            raw = await expect_response(loop, sock)
        finally:
            sock.close()

        assert raw[14] == int(MessageType.RESPONSE)
        assert raw[15] == int(ReturnCode.E_OK)
        assert ArrayPayload(4).deserialize(raw[16:]).values == data_b, (
            "reassembled message contains bytes from the abandoned session"
        )

        probe = await call(ets_client, METHOD_ECHO_UINT8, b"\x42")
        assert_ok(probe)

    # If the final segment (more_segments=0) does not arrive, the server does not
    # deliver the message to the application. The message also has no timeout,
    # because the cleanup timer is not active. This test shows only that the server
    # does not crash and does not respond too early. It cannot check the memory leak.
    async def test_incomplete_message_never_delivered_no_response(self, ets_client, host_ip, session_id):
        content = ArrayPayload(4, bytes(range(16))).serialize()
        first_segment_only = content[:16]

        sock = open_bound_socket(_TP_TEST_PORT)
        loop = asyncio.get_running_loop()
        try:
            sock.sendto(
                tp_segment(METHOD_ECHO_UINT8_ARRAY_16BIT_LENGTH, session_id, 0, first_segment_only, True),
                (host_ip, ETS_SERVER_UDP_PORT),
            )
            await expect_no_response(loop, sock)
        finally:
            sock.close()

        probe = await call(ets_client, METHOD_ECHO_UINT8, b"\x42")
        assert_ok(probe)

    # The server drops a segment if the header `length` field is different from
    # the datagram size. The test does not use tp_segment, because that helper
    # always calculates a correct length field.
    async def test_segment_length_field_mismatch_dropped(self, ets_client, host_ip, session_id):
        segment_payload = bytes(16)
        true_body_len = 8 + TP_HEADER_SIZE + len(segment_payload)  # 28
        lying_body_len = true_body_len + 64  # the header length is much larger than the data sent

        header = struct.pack(
            ">HHIHHBBBB",
            ETS_SERVICE_ID,
            METHOD_ECHO_UINT8_ARRAY_16BIT_LENGTH,
            lying_body_len,
            _CLIENT_ID,
            session_id,
            0x01,
            ETS_MAJOR_VERSION,
            int(MessageType.REQUEST) | TP_FLAG,
            0x00,
        )
        frame = header + tp_header(0, True) + segment_payload

        sock = open_bound_socket(_TP_TEST_PORT)
        loop = asyncio.get_running_loop()
        try:
            sock.sendto(frame, (host_ip, ETS_SERVER_UDP_PORT))
            await expect_no_response(loop, sock)
        finally:
            sock.close()

        probe = await call(ets_client, METHOD_ECHO_UINT8, b"\x42")
        assert_ok(probe)

    # Two different checks reject segments that make the reassembled message
    # larger than max_message_size_ (50000). `offset` is only a header field.
    # Thus the tests do not send 50000 bytes.
    async def test_offset_exceeds_max_message_size_dropped(self, ets_client, host_ip, session_id):
        # offset (50000) > max_message_size_ - SOMEIP_FULL_HEADER_SIZE (49984).
        frame = tp_segment(
            METHOD_ECHO_UINT8_ARRAY_16BIT_LENGTH,
            session_id,
            byte_offset=_MAX_MESSAGE_SIZE,
            segment_payload=bytes(16),
            more_segments=True,
        )

        sock = open_bound_socket(_TP_TEST_PORT)
        loop = asyncio.get_running_loop()
        try:
            sock.sendto(frame, (host_ip, ETS_SERVER_UDP_PORT))
            await expect_no_response(loop, sock)
        finally:
            sock.close()

        probe = await call(ets_client, METHOD_ECHO_UINT8, b"\x42")
        assert_ok(probe)

    async def test_reassembled_size_would_exceed_max_message_size_dropped(self, ets_client, host_ip, session_id):
        # offset (49968) passes the offset check (<= 49984).
        # But SOMEIP_FULL_HEADER_SIZE + offset + segment_size (16+49968+48 = 50032)
        # is more than max_message_size_ (50000). Thus the second check fails.
        offset = 49968
        segment_payload = bytes(48)
        assert _SOMEIP_HEADER_SIZE + offset + len(segment_payload) > _MAX_MESSAGE_SIZE
        frame = tp_segment(
            METHOD_ECHO_UINT8_ARRAY_16BIT_LENGTH,
            session_id,
            byte_offset=offset,
            segment_payload=segment_payload,
            more_segments=True,
        )

        sock = open_bound_socket(_TP_TEST_PORT)
        loop = asyncio.get_running_loop()
        try:
            sock.sendto(frame, (host_ip, ETS_SERVER_UDP_PORT))
            await expect_no_response(loop, sock)
        finally:
            sock.close()

        probe = await call(ets_client, METHOD_ECHO_UINT8, b"\x42")
        assert_ok(probe)

    # The server also does the length check when it adds a segment to a pending
    # message. A valid first segment starts a pending message. The server must
    # drop a too-short *second* frame for the same session and keep the pending
    # message. The message must complete when the real final segment arrives.
    @pytest.mark.xfail(strict=True, reason=_LGSOMEIP_5_XFAIL_REASON)
    async def test_second_segment_too_short_for_tp_header_dropped(self, ets_client, host_ip, session_id):
        method = METHOD_ECHO_UINT8_ARRAY_16BIT_LENGTH
        data = bytes(range(16))
        content = ArrayPayload(4, data).serialize()
        first, second = content[:16], content[16:]

        body_len = 8 + 2
        too_short = (
            struct.pack(
                ">HHIHHBBBB",
                ETS_SERVICE_ID,
                method,
                body_len,
                _CLIENT_ID,
                session_id,
                0x01,
                ETS_MAJOR_VERSION,
                int(MessageType.REQUEST) | TP_FLAG,
                0x00,
            )
            + b"\x00\x00"
        )

        sock = open_bound_socket(_TP_TEST_PORT)
        loop = asyncio.get_running_loop()
        try:
            sock.sendto(tp_segment(method, session_id, 0, first, True), (host_ip, ETS_SERVER_UDP_PORT))
            sock.sendto(too_short, (host_ip, ETS_SERVER_UDP_PORT))
            sock.sendto(tp_segment(method, session_id, 16, second, False), (host_ip, ETS_SERVER_UDP_PORT))
            raw = await expect_response(loop, sock)
        finally:
            sock.close()

        assert raw[14] == int(MessageType.RESPONSE)
        assert raw[15] == int(ReturnCode.E_OK)
        assert ArrayPayload(4).deserialize(raw[16:]).values == data, (
            "malformed too-short frame corrupted the pending reassembly"
        )

        probe = await call(ets_client, METHOD_ECHO_UINT8, b"\x42")
        assert_ok(probe)

    # Segments that arrive out of order (1, 3, 2) must reassemble correctly.
    # Segment 1 starts the message. The server keeps segment 3 as a "future segment".
    # Segment 2 fills the gap, and then the server adds the kept segment 3.
    @pytest.mark.xfail(strict=True, reason=_LGSOMEIP_5_XFAIL_REASON)
    async def test_out_of_order_segments_still_reassemble(self, ets_client, host_ip, session_id):
        method = METHOD_ECHO_UINT8_ARRAY_16BIT_LENGTH
        data = bytes(range(32))
        content = ArrayPayload(4, data).serialize()  # 4 + 32 = 36 bytes
        seg1, seg2, seg3 = content[0:16], content[16:32], content[32:36]

        sock = open_bound_socket(_TP_TEST_PORT)
        loop = asyncio.get_running_loop()
        try:
            sock.sendto(tp_segment(method, session_id, 0, seg1, True), (host_ip, ETS_SERVER_UDP_PORT))
            sock.sendto(tp_segment(method, session_id, 32, seg3, False), (host_ip, ETS_SERVER_UDP_PORT))
            sock.sendto(tp_segment(method, session_id, 16, seg2, True), (host_ip, ETS_SERVER_UDP_PORT))
            raw = await expect_response(loop, sock)
        finally:
            sock.close()

        assert raw[14] == int(MessageType.RESPONSE)
        assert raw[15] == int(ReturnCode.E_OK)
        assert ArrayPayload(4).deserialize(raw[16:]).values == data, (
            "out-of-order delivery produced an incorrect reassembly"
        )

        probe = await call(ets_client, METHOD_ECHO_UINT8, b"\x42")
        assert_ok(probe)

    # The first segment that arrives has an offset that is not zero (2, 1, 3).
    # The server keeps it as a future segment. Segment 1 and the gap fill then
    # add all data in the correct order.
    @pytest.mark.xfail(strict=True, reason=_LGSOMEIP_5_XFAIL_REASON)
    async def test_first_segment_out_of_order_still_reassembles(self, ets_client, host_ip, session_id):
        method = METHOD_ECHO_UINT8_ARRAY_16BIT_LENGTH
        data = bytes(range(32))
        content = ArrayPayload(4, data).serialize()  # 4 + 32 = 36 bytes
        seg1, seg2, seg3 = content[0:16], content[16:32], content[32:36]

        sock = open_bound_socket(_TP_TEST_PORT)
        loop = asyncio.get_running_loop()
        try:
            # Segment 2 arrives first.
            sock.sendto(tp_segment(method, session_id, 16, seg2, True), (host_ip, ETS_SERVER_UDP_PORT))
            sock.sendto(tp_segment(method, session_id, 0, seg1, True), (host_ip, ETS_SERVER_UDP_PORT))
            sock.sendto(tp_segment(method, session_id, 32, seg3, False), (host_ip, ETS_SERVER_UDP_PORT))
            raw = await expect_response(loop, sock)
        finally:
            sock.close()

        assert raw[14] == int(MessageType.RESPONSE)
        assert raw[15] == int(ReturnCode.E_OK)
        assert ArrayPayload(4).deserialize(raw[16:]).values == data, (
            "out-of-order delivery produced an incorrect reassembly"
        )

        probe = await call(ets_client, METHOD_ECHO_UINT8, b"\x42")
        assert_ok(probe)

    # A gap in the *middle* of the stream (segment 2 of 3 does not arrive).
    # The message also stays in memory, as when the final segment is missing.
    # Segment 3 has more_segments=0, so the server knows the last segment.
    # But nothing fills the gap at offset 16, so the message is never complete.
    async def test_gap_in_middle_never_delivered_no_response(self, ets_client, host_ip, session_id):
        method = METHOD_ECHO_UINT8_ARRAY_16BIT_LENGTH
        data = bytes(range(32))
        content = ArrayPayload(4, data).serialize()
        seg1, seg3 = content[0:16], content[32:36]

        sock = open_bound_socket(_TP_TEST_PORT)
        loop = asyncio.get_running_loop()
        try:
            sock.sendto(tp_segment(method, session_id, 0, seg1, True), (host_ip, ETS_SERVER_UDP_PORT))
            # The test does not send segment 2 (offset 16).
            sock.sendto(tp_segment(method, session_id, 32, seg3, False), (host_ip, ETS_SERVER_UDP_PORT))
            await expect_no_response(loop, sock)
        finally:
            sock.close()

        probe = await call(ets_client, METHOD_ECHO_UINT8, b"\x42")
        assert_ok(probe)

    # Two TP reassemblies for different methods have different bucket keys.
    # The bucket key is service+method+client+iface_ver+msg_type.
    # The test sends their segments alternately on the same socket.
    # The data of one message must not go into the other message.
    @pytest.mark.xfail(strict=True, reason=_LGSOMEIP_5_XFAIL_REASON)
    async def test_concurrent_different_method_buckets_do_not_cross_contaminate(self, ets_client, host_ip, session_id):
        method_a = METHOD_ECHO_UINT8_ARRAY_16BIT_LENGTH
        method_b = METHOD_ECHO_UINT8_ARRAY
        session_a = session_id
        session_b = next(_session_ids)

        data_a = bytes([0xAA] * 16)
        data_b = bytes([0xBB] * 16)
        content_a = ArrayPayload(4, data_a).serialize()
        content_b = ArrayPayload(4, data_b).serialize()

        sock = open_bound_socket(_TP_TEST_PORT)
        loop = asyncio.get_running_loop()
        try:
            sock.sendto(tp_segment(method_a, session_a, 0, content_a[:16], True), (host_ip, ETS_SERVER_UDP_PORT))
            sock.sendto(tp_segment(method_b, session_b, 0, content_b[:16], True), (host_ip, ETS_SERVER_UDP_PORT))
            sock.sendto(tp_segment(method_a, session_a, 16, content_a[16:], False), (host_ip, ETS_SERVER_UDP_PORT))
            sock.sendto(tp_segment(method_b, session_b, 16, content_b[16:], False), (host_ip, ETS_SERVER_UDP_PORT))

            responses = {}
            for _ in range(2):
                raw = await expect_response(loop, sock)
                responses[raw[2:4]] = raw  # method_id is in bytes 2-3
        finally:
            sock.close()

        raw_a = responses[struct.pack(">H", method_a)]
        raw_b = responses[struct.pack(">H", method_b)]
        assert ArrayPayload(4).deserialize(raw_a[16:]).values == data_a, (
            "method A's response contains bytes from method B's message"
        )
        assert ArrayPayload(4).deserialize(raw_b[16:]).values == data_b, (
            "method B's response contains bytes from method A's message"
        )

        probe = await call(ets_client, METHOD_ECHO_UINT8, b"\x42")
        assert_ok(probe)
