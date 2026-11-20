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
"""TC8 SD robustness tests for malformed SOME/IP-SD messages.

Each test sends one malformed SD message to someipd and checks the reply.
Then the test makes sure that the DUT still sends its next cyclic multicast OfferService.
This shows that the DUT did not crash and that its SD stack still operates.

The OA liveness check sends a FindService and waits for a unicast OfferService.
The DUT does not send a unicast OfferService in reply to a FindService.
Thus the tests use the cyclic multicast offer as the liveness check.
"""

import socket
import time

import pytest
from attribute_plugin import add_test_properties
from someip_itf.codec import SdEntryType
from someip_itf.tc8.asserts import (
    assert_ack_or_silent,
    assert_ack_received,
    assert_nack_received,
    assert_no_sd_reply,
    assert_no_subscribe_ack,
    assert_no_subscribe_reply,
)
from someip_itf.tc8.constants import (
    EVENTGROUP_ID,
    INSTANCE_ID,
    PROBE_SUBSCRIBER_PORT,
    SD_PORT,
    SERVICE_ID,
    UNKNOWN_SERVICE_ID,
)
from someip_itf.tc8.dut_lifecycle import TargetStack
from someip_itf.tc8.probes import DUT_ALIVE_TIMEOUT, verify_dut_alive
from someip_itf.tc8.sd_malformed import (
    send_sd_empty_entries,
    send_sd_empty_option,
    send_sd_empty_options_array,
    send_sd_entries_length_too_long_padded,
    send_sd_entries_length_too_short,
    send_sd_entries_length_wrong,
    send_sd_entry_refs_more_options,
    send_sd_entry_same_option_twice,
    send_sd_entry_unknown_option_type,
    send_sd_find_with_options,
    send_sd_option_length_too_long,
    send_sd_option_length_too_short,
    send_sd_option_pair_unaligned,
    send_sd_oversized_entries_length,
    send_sd_subscribe_endpoint_ip,
    send_sd_subscribe_endpoint_reserved_bits_set,
    send_sd_subscribe_nonexistent_service,
    send_sd_subscribe_port_zero,
    send_sd_subscribe_wrong_l4proto,
    send_sd_subscribe_wrong_major_version,
    send_sd_subscribe_wrong_someip_length,
    send_sd_two_option_array_length,
    send_sd_two_options_one_unknown_type,
    send_sd_wrong_someip_message_id,
)
from someip_itf.tc8.sd_sender import capture_unicast_sd_entries, open_sender_socket

_LGSOMEIP_7_XFAIL_REASON = (
    "[lg-someip #7] the daemon discards an SD message with a malformed or unknown option instead of NAcking the "
    "Subscribe"
)

#: An instance and an eventgroup the DUT never offers.
_UNKNOWN_INSTANCE_ID: int = 0xBEEF
_UNKNOWN_EVENTGROUP_ID: int = 0xDEAD


@pytest.fixture
def sender(tc8_tester_ip: str) -> socket.socket:
    """Open a new UDP sender socket bound to tc8_tester_ip:SD_PORT for each test.

    The fixture has function scope. A shared socket can keep a late reply from one test in its receive buffer.
    The next test can then get that reply and connect it to the wrong injected message.
    """
    sock = open_sender_socket(tc8_tester_ip)
    yield sock
    sock.close()


class TestSDMalformedEntries:
    """Tests for a malformed entries array.

    For four cases, the OA test expects a NAck, but someipd discards the message.
    For each of these cases, a strict xfail test asserts the NAck.
    A companion test asserts the discard, which is the current DUT behavior.
    In one case the entries array goes past the end of the message. The specification discards such a message.
    In two cases the DUT reads the options array length from entry bytes, and the array goes past the message.
    In one case the entries array length is not a multiple of the 16-byte entry size.
    """

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_robustness"],
        partially_verifies=["SOMEIP_ETS_111"],
        test_type="fault-injection",
        derivation_technique="error-guessing",
    )
    def test_ets_111_empty_entries_array(
        self,
        someipd_dut: TargetStack,
        sender: socket.socket,
        tc8_host_ip: str,
        tc8_dut_ip: str,
    ) -> None:
        """SD message with entries array length 0. The DUT must not react."""
        assert someipd_dut.poll() is None, "DUT is not running before injection"
        send_sd_empty_entries(sender, (tc8_dut_ip, SD_PORT))
        assert_no_sd_reply(sender)
        verify_dut_alive(tc8_host_ip)

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_robustness", "AS__PRS_SOMEIPSD_00393"],
        partially_verifies=["AS__PRS_SOMEIPSD_00130", "AS__PRS_SOMEIPSD_00234", "SOMEIP_ETS_112"],
        test_type="fault-injection",
        derivation_technique="error-guessing",
    )
    @pytest.mark.xfail(strict=True, reason=_LGSOMEIP_7_XFAIL_REASON)
    def test_ets_112_empty_option_zero_length(
        self,
        someipd_dut: TargetStack,
        sender: socket.socket,
        tc8_host_ip: str,
        tc8_dut_ip: str,
    ) -> None:
        """SubscribeEventgroup with IPv4Option length=0. The DUT must send an explicit NAck."""
        assert someipd_dut.poll() is None, "DUT is not running before injection"
        send_sd_empty_option(sender, (tc8_dut_ip, SD_PORT), SERVICE_ID, INSTANCE_ID, EVENTGROUP_ID)
        assert_nack_received(sender, SERVICE_ID, INSTANCE_ID, EVENTGROUP_ID)
        verify_dut_alive(tc8_host_ip)

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_robustness", "AS__PRS_SOMEIPSD_00393"],
        partially_verifies=["AS__PRS_SOMEIPSD_00130", "AS__PRS_SOMEIPSD_00234", "SOMEIP_ETS_113"],
        test_type="fault-injection",
        derivation_technique="error-guessing",
    )
    def test_ets_113_empty_options_array(
        self,
        someipd_dut: TargetStack,
        sender: socket.socket,
        tc8_host_ip: str,
        tc8_dut_ip: str,
    ) -> None:
        """SubscribeEventgroup entry refers to one option, but options_array_length=0.

        The option that the entry refers to does not exist. The DUT must send an explicit NAck.
        """
        assert someipd_dut.poll() is None, "DUT is not running before injection"
        send_sd_empty_options_array(sender, (tc8_dut_ip, SD_PORT), SERVICE_ID, INSTANCE_ID, EVENTGROUP_ID)
        assert_nack_received(sender, SERVICE_ID, INSTANCE_ID, EVENTGROUP_ID)
        verify_dut_alive(tc8_host_ip)

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_robustness"],
        partially_verifies=["AS__PRS_SOMEIPSD_00264", "AS__PRS_SOMEIPSD_00265"],
        test_type="fault-injection",
        derivation_technique="error-guessing",
    )
    def test_entries_length_wrong_combined_discarded(
        self,
        someipd_dut: TargetStack,
        sender: socket.socket,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        tc8_tester_ip: str,
    ) -> None:
        """Two correct Subscribe entries (32 bytes) with an entries array length of 16 get no SD reply.

        This test asserts the current DUT behavior: someipd discards the message.
        test_ets_114_entries_length_wrong_combined_nacked asserts the NAck that the OA test expects.
        """
        assert someipd_dut.poll() is None, "DUT is not running before injection"
        send_sd_entries_length_wrong(
            sender,
            (tc8_dut_ip, SD_PORT),
            SERVICE_ID,
            INSTANCE_ID,
            EVENTGROUP_ID,
            tc8_tester_ip,
            PROBE_SUBSCRIBER_PORT,
        )
        assert_no_sd_reply(sender)
        verify_dut_alive(tc8_host_ip)

    @pytest.mark.xfail(
        strict=True,
        raises=AssertionError,
        reason="someipd discards a message that it cannot parse, so it sends no SubscribeEventgroupNAck",
    )
    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_robustness"],
        partially_verifies=["SOMEIP_ETS_114"],
        test_type="fault-injection",
        derivation_technique="error-guessing",
    )
    def test_ets_114_entries_length_wrong_combined_nacked(
        self,
        someipd_dut: TargetStack,
        sender: socket.socket,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        tc8_tester_ip: str,
    ) -> None:
        """Two correct Subscribe entries with an entries array length of 16 get a NAck.

        test_entries_length_wrong_combined_discarded asserts the current DUT behavior, which is no reply.
        """
        assert someipd_dut.poll() is None, "DUT is not running before injection"
        send_sd_entries_length_wrong(
            sender,
            (tc8_dut_ip, SD_PORT),
            SERVICE_ID,
            INSTANCE_ID,
            EVENTGROUP_ID,
            tc8_tester_ip,
            PROBE_SUBSCRIBER_PORT,
        )
        assert_nack_received(sender, SERVICE_ID, INSTANCE_ID, EVENTGROUP_ID)
        verify_dut_alive(tc8_host_ip)

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_robustness", "AS__PRS_SOMEIPSD_00393"],
        partially_verifies=[
            "AS__PRS_SOMEIPSD_00130",
            "AS__PRS_SOMEIPSD_00234",
            "AS__PRS_SOMEIPSD_00566",
            "SOMEIP_ETS_115",
        ],
        test_type="fault-injection",
        derivation_technique="error-guessing",
    )
    def test_ets_115_entry_refs_more_options_than_exist(
        self,
        someipd_dut: TargetStack,
        sender: socket.socket,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        tc8_tester_ip: str,
    ) -> None:
        """Entry has num_options_1=3, but the options array has only 1 option. The DUT must send a NAck."""
        assert someipd_dut.poll() is None, "DUT is not running before injection"
        send_sd_entry_refs_more_options(
            sender,
            (tc8_dut_ip, SD_PORT),
            SERVICE_ID,
            INSTANCE_ID,
            EVENTGROUP_ID,
            tc8_tester_ip,
            PROBE_SUBSCRIBER_PORT,
        )
        assert_nack_received(sender, SERVICE_ID, INSTANCE_ID, EVENTGROUP_ID)
        verify_dut_alive(tc8_host_ip)

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_robustness", "AS__PRS_SOMEIPSD_00393"],
        partially_verifies=["AS__PRS_SOMEIPSD_00130", "AS__PRS_SOMEIPSD_00234", "SOMEIP_ETS_116"],
        test_type="fault-injection",
        derivation_technique="error-guessing",
    )
    @pytest.mark.xfail(strict=True, reason=_LGSOMEIP_7_XFAIL_REASON)
    def test_ets_116_entry_unknown_option_type(
        self,
        someipd_dut: TargetStack,
        sender: socket.socket,
        tc8_host_ip: str,
        tc8_dut_ip: str,
    ) -> None:
        """SubscribeEventgroup with unknown option type 0x77. The DUT must send an explicit NAck."""
        assert someipd_dut.poll() is None, "DUT is not running before injection"
        send_sd_entry_unknown_option_type(sender, (tc8_dut_ip, SD_PORT), SERVICE_ID, INSTANCE_ID, EVENTGROUP_ID)
        assert_nack_received(sender, SERVICE_ID, INSTANCE_ID, EVENTGROUP_ID)
        verify_dut_alive(tc8_host_ip)

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_robustness"],
        partially_verifies=["AS__PRS_SOMEIPSD_00130", "SOMEIP_ETS_117"],
        test_type="fault-injection",
        derivation_technique="error-guessing",
    )
    def test_ets_117_two_entries_same_option(
        self,
        someipd_dut: TargetStack,
        sender: socket.socket,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        tc8_tester_ip: str,
    ) -> None:
        """One entry refers to two options of the same kind (both IPv4EndpointOption).

        The DUT can send a NAck or ignore the request. Only an accepting Ack is a failure.
        """
        assert someipd_dut.poll() is None, "DUT is not running before injection"
        send_sd_entry_same_option_twice(
            sender,
            (tc8_dut_ip, SD_PORT),
            SERVICE_ID,
            INSTANCE_ID,
            EVENTGROUP_ID,
            tc8_tester_ip,
            PROBE_SUBSCRIBER_PORT,
        )
        assert_no_subscribe_ack(sender, SERVICE_ID, INSTANCE_ID, EVENTGROUP_ID)
        verify_dut_alive(tc8_host_ip)

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_robustness"],
        partially_verifies=[
            "AS__PRS_SOMEIPSD_00232",
            "AS__PRS_SOMEIPSD_00422",
            "AS__PRS_SOMEIPSD_00529",
            "SOMEIP_ETS_118",
        ],
        test_type="fault-injection",
        derivation_technique="error-guessing",
    )
    def test_ets_118_find_service_with_endpoint_option(
        self,
        someipd_dut: TargetStack,
        sender: socket.socket,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        tc8_tester_ip: str,
    ) -> None:
        """10 FindService messages (100 ms apart) with an unexpected endpoint option.

        An endpoint option has no meaning in a FindService. Thus the DUT must still send a unicast OfferService.
        """
        assert someipd_dut.poll() is None, "DUT is not running before injection"
        for _ in range(10):
            send_sd_find_with_options(sender, (tc8_dut_ip, SD_PORT), SERVICE_ID, tc8_tester_ip, PROBE_SUBSCRIBER_PORT)
            time.sleep(0.1)
        entries = capture_unicast_sd_entries(
            sender, filter_types=(SdEntryType.OfferService,), timeout_secs=DUT_ALIVE_TIMEOUT
        )
        assert any(e.service_id == SERVICE_ID for e in entries), (
            "expected at least one unicast OfferService in reply to the FindService burst"
        )
        verify_dut_alive(tc8_host_ip)

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_robustness"],
        partially_verifies=["AS__PRS_SOMEIPSD_00265", "AS__PRS_SOMEIPSD_00803"],
        test_type="fault-injection",
        derivation_technique="error-guessing",
    )
    def test_entries_length_wildly_too_large_discarded(
        self,
        someipd_dut: TargetStack,
        sender: socket.socket,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        tc8_tester_ip: str,
    ) -> None:
        """An entries array length of 0xFFFF, past the end of the message, gets no SD reply.

        This test asserts the current DUT behavior: someipd discards the message.
        test_ets_123_entries_length_wildly_too_large_nacked asserts the NAck that the OA test expects.
        """
        assert someipd_dut.poll() is None, "DUT is not running before injection"
        send_sd_oversized_entries_length(
            sender,
            (tc8_dut_ip, SD_PORT),
            SERVICE_ID,
            INSTANCE_ID,
            EVENTGROUP_ID,
            tc8_tester_ip,
            PROBE_SUBSCRIBER_PORT,
        )
        assert_no_sd_reply(sender)
        verify_dut_alive(tc8_host_ip)

    @pytest.mark.xfail(
        strict=True,
        raises=AssertionError,
        reason="someipd discards a message that it cannot parse, so it sends no SubscribeEventgroupNAck",
    )
    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_robustness"],
        partially_verifies=["SOMEIP_ETS_123"],
        test_type="fault-injection",
        derivation_technique="error-guessing",
    )
    def test_ets_123_entries_length_wildly_too_large_nacked(
        self,
        someipd_dut: TargetStack,
        sender: socket.socket,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        tc8_tester_ip: str,
    ) -> None:
        """An entries array length of 0xFFFF past the end of the message gets a NAck.

        test_entries_length_wildly_too_large_discarded asserts the current DUT behavior, which is no reply.
        """
        assert someipd_dut.poll() is None, "DUT is not running before injection"
        send_sd_oversized_entries_length(
            sender,
            (tc8_dut_ip, SD_PORT),
            SERVICE_ID,
            INSTANCE_ID,
            EVENTGROUP_ID,
            tc8_tester_ip,
            PROBE_SUBSCRIBER_PORT,
        )
        assert_nack_received(sender, SERVICE_ID, INSTANCE_ID, EVENTGROUP_ID)
        verify_dut_alive(tc8_host_ip)

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_robustness"],
        test_type="fault-injection",
        derivation_technique="error-guessing",
    )
    def test_entries_length_too_long_padded_discarded(
        self,
        someipd_dut: TargetStack,
        sender: socket.socket,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        tc8_tester_ip: str,
    ) -> None:
        """An entries array length of 40 over one entry plus 24 zero bytes gets no SD reply.

        All declared bytes are present, but 40 is not a multiple of the 16-byte entry size.
        This test asserts the current DUT behavior: someipd discards the message.
        test_ets_124_entries_length_too_long_padded_nacked asserts the NAck that the OA test expects.
        """
        assert someipd_dut.poll() is None, "DUT is not running before injection"
        send_sd_entries_length_too_long_padded(
            sender,
            (tc8_dut_ip, SD_PORT),
            SERVICE_ID,
            INSTANCE_ID,
            EVENTGROUP_ID,
            tc8_tester_ip,
            PROBE_SUBSCRIBER_PORT,
        )
        assert_no_sd_reply(sender)
        verify_dut_alive(tc8_host_ip)

    @pytest.mark.xfail(
        strict=True,
        raises=AssertionError,
        reason="someipd discards a message that it cannot parse, so it sends no SubscribeEventgroupNAck",
    )
    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_robustness"],
        partially_verifies=["SOMEIP_ETS_124"],
        test_type="fault-injection",
        derivation_technique="error-guessing",
    )
    def test_ets_124_entries_length_too_long_padded_nacked(
        self,
        someipd_dut: TargetStack,
        sender: socket.socket,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        tc8_tester_ip: str,
    ) -> None:
        """An entries array length of 40 over one entry plus 24 zero bytes gets a NAck.

        test_entries_length_too_long_padded_discarded asserts the current DUT behavior, which is no reply.
        """
        assert someipd_dut.poll() is None, "DUT is not running before injection"
        send_sd_entries_length_too_long_padded(
            sender,
            (tc8_dut_ip, SD_PORT),
            SERVICE_ID,
            INSTANCE_ID,
            EVENTGROUP_ID,
            tc8_tester_ip,
            PROBE_SUBSCRIBER_PORT,
        )
        assert_nack_received(sender, SERVICE_ID, INSTANCE_ID, EVENTGROUP_ID)
        verify_dut_alive(tc8_host_ip)

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_robustness"],
        partially_verifies=["AS__PRS_SOMEIPSD_00265", "AS__PRS_SOMEIPSD_00270"],
        test_type="fault-injection",
        derivation_technique="error-guessing",
    )
    def test_entries_length_too_short_discarded(
        self,
        someipd_dut: TargetStack,
        sender: socket.socket,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        tc8_tester_ip: str,
    ) -> None:
        """An entries array length of 8 over a complete 16-byte Subscribe entry gets no SD reply.

        This test asserts the current DUT behavior: someipd discards the message.
        test_ets_125_entries_length_too_short_nacked asserts the NAck that the OA test expects.
        """
        assert someipd_dut.poll() is None, "DUT is not running before injection"
        send_sd_entries_length_too_short(
            sender,
            (tc8_dut_ip, SD_PORT),
            SERVICE_ID,
            INSTANCE_ID,
            EVENTGROUP_ID,
            tc8_tester_ip,
            PROBE_SUBSCRIBER_PORT,
        )
        assert_no_sd_reply(sender)
        verify_dut_alive(tc8_host_ip)

    @pytest.mark.xfail(
        strict=True,
        raises=AssertionError,
        reason="someipd discards a message that it cannot parse, so it sends no SubscribeEventgroupNAck",
    )
    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_robustness"],
        partially_verifies=["SOMEIP_ETS_125"],
        test_type="fault-injection",
        derivation_technique="error-guessing",
    )
    def test_ets_125_entries_length_too_short_nacked(
        self,
        someipd_dut: TargetStack,
        sender: socket.socket,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        tc8_tester_ip: str,
    ) -> None:
        """An entries array length of 8 over a complete 16-byte Subscribe entry gets a NAck.

        test_entries_length_too_short_discarded asserts the current DUT behavior, which is no reply.
        """
        assert someipd_dut.poll() is None, "DUT is not running before injection"
        send_sd_entries_length_too_short(
            sender,
            (tc8_dut_ip, SD_PORT),
            SERVICE_ID,
            INSTANCE_ID,
            EVENTGROUP_ID,
            tc8_tester_ip,
            PROBE_SUBSCRIBER_PORT,
        )
        assert_nack_received(sender, SERVICE_ID, INSTANCE_ID, EVENTGROUP_ID)
        verify_dut_alive(tc8_host_ip)


class TestSDMalformedOptions:
    """Tests for a malformed options array."""

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_robustness"],
        partially_verifies=["AS__PRS_SOMEIPSD_00130", "SOMEIP_ETS_134"],
        test_type="fault-injection",
        derivation_technique="error-guessing",
    )
    def test_ets_134_option_ends_past_options_array_var_a(
        self,
        someipd_dut: TargetStack,
        sender: socket.socket,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        tc8_tester_ip: str,
    ) -> None:
        """Two correct endpoint options, options array length cut 24 -> 12, SOME/IP length 60 -> 48.

        The OA test accepts a NAck or no reply. Only an accepting Ack is a failure.
        """
        assert someipd_dut.poll() is None, "DUT is not running before injection"
        send_sd_two_option_array_length(
            sender,
            (tc8_dut_ip, SD_PORT),
            SERVICE_ID,
            INSTANCE_ID,
            EVENTGROUP_ID,
            tc8_tester_ip,
            PROBE_SUBSCRIBER_PORT,
            options_length_override=12,
            someip_length_override=48,
        )
        assert_no_subscribe_ack(sender, SERVICE_ID, INSTANCE_ID, EVENTGROUP_ID)
        verify_dut_alive(tc8_host_ip)

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_robustness"],
        partially_verifies=["AS__PRS_SOMEIPSD_00130", "SOMEIP_ETS_135"],
        test_type="fault-injection",
        derivation_technique="error-guessing",
    )
    @pytest.mark.parametrize("option_length", [0x000A, 0x00FF], ids=["0x000A", "0x00FF"])
    def test_ets_135_option_ends_past_options_array_var_b(
        self,
        someipd_dut: TargetStack,
        sender: socket.socket,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        tc8_tester_ip: str,
        option_length: int,
    ) -> None:
        """The only endpoint option has a length that goes past the 12-byte options array.

        The OA test accepts a NAck or no reply. Only an accepting Ack is a failure.
        """
        assert someipd_dut.poll() is None, "DUT is not running before injection"
        send_sd_option_length_too_long(
            sender,
            (tc8_dut_ip, SD_PORT),
            SERVICE_ID,
            INSTANCE_ID,
            EVENTGROUP_ID,
            tc8_tester_ip,
            PROBE_SUBSCRIBER_PORT,
            option_length_override=option_length,
        )
        assert_no_subscribe_ack(sender, SERVICE_ID, INSTANCE_ID, EVENTGROUP_ID)
        verify_dut_alive(tc8_host_ip)

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_robustness"],
        partially_verifies=["AS__PRS_SOMEIPSD_00130", "SOMEIP_ETS_136"],
        test_type="fault-injection",
        derivation_technique="error-guessing",
    )
    def test_ets_136_option_length_too_short(
        self,
        someipd_dut: TargetStack,
        sender: socket.socket,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        tc8_tester_ip: str,
    ) -> None:
        """IPv4EndpointOption length field = 4 (shorter than the 9 that UDP requires).

        The DUT must send a NAck or ignore the request. Only an accepting Ack is a failure.
        """
        assert someipd_dut.poll() is None, "DUT is not running before injection"
        send_sd_option_length_too_short(
            sender,
            (tc8_dut_ip, SD_PORT),
            SERVICE_ID,
            INSTANCE_ID,
            EVENTGROUP_ID,
            tc8_tester_ip,
            PROBE_SUBSCRIBER_PORT,
        )
        assert_no_subscribe_ack(sender, SERVICE_ID, INSTANCE_ID, EVENTGROUP_ID)
        verify_dut_alive(tc8_host_ip)

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_robustness", "AS__PRS_SOMEIPSD_00393"],
        partially_verifies=[
            "AS__PRS_SOMEIPSD_00130",
            "AS__PRS_SOMEIPSD_00234",
            "AS__PRS_SOMEIPSD_00394",
            "AS__PRS_SOMEIPSD_00566",
            "SOMEIP_ETS_137",
        ],
        test_type="fault-injection",
        derivation_technique="error-guessing",
    )
    @pytest.mark.xfail(strict=True, reason=_LGSOMEIP_7_XFAIL_REASON)
    def test_ets_137_option_length_unaligned(
        self,
        someipd_dut: TargetStack,
        sender: socket.socket,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        tc8_tester_ip: str,
    ) -> None:
        """A TCP option padded to length 14 and a UDP option cut to length 4, together still 24 bytes.

        The five bytes cut from the UDP option pad the TCP option. Thus the options array keeps its 24-byte size.
        The DUT must send an explicit NAck.
        """
        assert someipd_dut.poll() is None, "DUT is not running before injection"
        send_sd_option_pair_unaligned(
            sender,
            (tc8_dut_ip, SD_PORT),
            SERVICE_ID,
            INSTANCE_ID,
            EVENTGROUP_ID,
            tc8_tester_ip,
            PROBE_SUBSCRIBER_PORT,
        )
        assert_nack_received(sender, SERVICE_ID, INSTANCE_ID, EVENTGROUP_ID)
        verify_dut_alive(tc8_host_ip)

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_robustness"],
        partially_verifies=["SOMEIP_ETS_138"],
        test_type="fault-injection",
        derivation_technique="error-guessing",
    )
    def test_ets_138_options_array_length_too_large(
        self,
        someipd_dut: TargetStack,
        sender: socket.socket,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        tc8_tester_ip: str,
    ) -> None:
        """Options array length 0x28 over two correct endpoint options (0x18 bytes).

        The OA test expects an Ack and accepts no reply for AUTOSAR compatibility. Only a NAck is a failure.
        """
        assert someipd_dut.poll() is None, "DUT is not running before injection"
        send_sd_two_option_array_length(
            sender,
            (tc8_dut_ip, SD_PORT),
            SERVICE_ID,
            INSTANCE_ID,
            EVENTGROUP_ID,
            tc8_tester_ip,
            PROBE_SUBSCRIBER_PORT,
            options_length_override=0x28,
        )
        assert_ack_or_silent(sender, SERVICE_ID, INSTANCE_ID, EVENTGROUP_ID)
        verify_dut_alive(tc8_host_ip)

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_robustness", "AS__PRS_SOMEIPSD_00393"],
        partially_verifies=[
            "AS__PRS_SOMEIPSD_00130",
            "AS__PRS_SOMEIPSD_00234",
            "AS__PRS_SOMEIPSD_00265",
            "AS__PRS_SOMEIPSD_00566",
            "SOMEIP_ETS_139",
        ],
        test_type="fault-injection",
        derivation_technique="error-guessing",
    )
    @pytest.mark.xfail(strict=True, reason=_LGSOMEIP_7_XFAIL_REASON)
    def test_ets_139_options_array_length_too_short(
        self,
        someipd_dut: TargetStack,
        sender: socket.socket,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        tc8_tester_ip: str,
    ) -> None:
        """Options array length 2 over two endpoint options (24 bytes). Both options are outside the array.

        The DUT must send an explicit NAck.
        """
        assert someipd_dut.poll() is None, "DUT is not running before injection"
        send_sd_two_option_array_length(
            sender,
            (tc8_dut_ip, SD_PORT),
            SERVICE_ID,
            INSTANCE_ID,
            EVENTGROUP_ID,
            tc8_tester_ip,
            PROBE_SUBSCRIBER_PORT,
            options_length_override=2,
        )
        assert_nack_received(sender, SERVICE_ID, INSTANCE_ID, EVENTGROUP_ID)
        verify_dut_alive(tc8_host_ip)

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_robustness", "AS__PRS_SOMEIPSD_00393"],
        partially_verifies=[
            "AS__PRS_SOMEIPSD_00130",
            "AS__PRS_SOMEIPSD_00234",
            "AS__PRS_SOMEIPSD_00566",
            "SOMEIP_ETS_174",
        ],
        test_type="fault-injection",
        derivation_technique="error-guessing",
    )
    @pytest.mark.xfail(strict=True, reason=_LGSOMEIP_7_XFAIL_REASON)
    def test_ets_174_unknown_option_type_0x77(
        self,
        someipd_dut: TargetStack,
        sender: socket.socket,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        tc8_tester_ip: str,
    ) -> None:
        """Entry refers to a valid IPv4EndpointOption and an option of unknown type 0x77.

        test_ets_116_entry_unknown_option_type sends only one bad option. Here a usable option is also present.
        The DUT must send an explicit NAck.
        """
        assert someipd_dut.poll() is None, "DUT is not running before injection"
        send_sd_two_options_one_unknown_type(
            sender,
            (tc8_dut_ip, SD_PORT),
            SERVICE_ID,
            INSTANCE_ID,
            EVENTGROUP_ID,
            tc8_tester_ip,
            PROBE_SUBSCRIBER_PORT,
        )
        assert_nack_received(sender, SERVICE_ID, INSTANCE_ID, EVENTGROUP_ID)
        verify_dut_alive(tc8_host_ip)


class TestSDSubscribeEdgeCases:
    """Tests for edge cases in SubscribeEventgroup messages."""

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_robustness", "AS__PRS_SOMEIPSD_00393"],
        partially_verifies=[
            "AS__PRS_SOMEIPSD_00130",
            "AS__PRS_SOMEIPSD_00234",
            "AS__PRS_SOMEIPSD_00394",
            "AS__PRS_SOMEIPSD_00566",
            "SOMEIP_ETS_109",
        ],
        test_type="fault-injection",
        derivation_technique="error-guessing",
    )
    def test_ets_109_subscribe_no_endpoint_option(
        self,
        someipd_dut: TargetStack,
        sender: socket.socket,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        tc8_tester_ip: str,
    ) -> None:
        """SubscribeEventgroup with the IPv4EndpointOption present but its port set to 0.

        The DUT must send an explicit NAck.
        """
        assert someipd_dut.poll() is None, "DUT is not running before injection"
        send_sd_subscribe_port_zero(
            sender, (tc8_dut_ip, SD_PORT), SERVICE_ID, INSTANCE_ID, EVENTGROUP_ID, tc8_tester_ip
        )
        assert_nack_received(sender, SERVICE_ID, INSTANCE_ID, EVENTGROUP_ID)
        verify_dut_alive(tc8_host_ip)

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_robustness", "AS__PRS_SOMEIPSD_00393"],
        partially_verifies=[
            "AS__PRS_SOMEIPSD_00130",
            "AS__PRS_SOMEIPSD_00234",
            "AS__PRS_SOMEIPSD_00394",
            "AS__PRS_SOMEIPSD_00566",
            "SOMEIP_ETS_110",
        ],
        test_type="fault-injection",
        derivation_technique="error-guessing",
    )
    @pytest.mark.parametrize("endpoint_ip", ["0.0.0.0", "32.0.0.0"], ids=["0.0.0.0", "32.0.0.0"])
    def test_ets_110_subscribe_endpoint_ip_unspecified(
        self,
        someipd_dut: TargetStack,
        sender: socket.socket,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        endpoint_ip: str,
    ) -> None:
        """SubscribeEventgroup whose endpoint option names no usable IPv4 address.

        The OA tester sends 32.0.0.0 as the substitute. 0.0.0.0 is the literal unspecified address.
        The DUT must send an explicit NAck.
        """
        assert someipd_dut.poll() is None, "DUT is not running before injection"
        send_sd_subscribe_endpoint_ip(
            sender,
            (tc8_dut_ip, SD_PORT),
            SERVICE_ID,
            INSTANCE_ID,
            EVENTGROUP_ID,
            endpoint_ip=endpoint_ip,
            subscriber_port=PROBE_SUBSCRIBER_PORT,
        )
        assert_nack_received(sender, SERVICE_ID, INSTANCE_ID, EVENTGROUP_ID)
        verify_dut_alive(tc8_host_ip)

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_robustness", "AS__PRS_SOMEIPSD_00393"],
        partially_verifies=["AS__PRS_SOMEIPSD_00394", "AS__PRS_SOMEIPSD_00566", "SOMEIP_ETS_154"],
        test_type="fault-injection",
        derivation_technique="error-guessing",
    )
    def test_ets_154_subscribe_endpoint_ip_unreachable(
        self,
        someipd_dut: TargetStack,
        sender: socket.socket,
        tc8_host_ip: str,
        tc8_dut_ip: str,
    ) -> None:
        """SubscribeEventgroup with an unreachable, well-formed IPv4 endpoint address.

        test_ets_110_subscribe_endpoint_ip_unspecified uses an unspecified address.
        This address looks routable, but the DUT cannot send to it. The DUT must send an explicit NAck.
        """
        assert someipd_dut.poll() is None, "DUT is not running before injection"
        send_sd_subscribe_endpoint_ip(
            sender, (tc8_dut_ip, SD_PORT), SERVICE_ID, INSTANCE_ID, EVENTGROUP_ID, "169.254.1.1", PROBE_SUBSCRIBER_PORT
        )
        assert_nack_received(sender, SERVICE_ID, INSTANCE_ID, EVENTGROUP_ID)
        verify_dut_alive(tc8_host_ip)

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_robustness", "AS__PRS_SOMEIPSD_00393"],
        partially_verifies=["AS__PRS_SOMEIPSD_00394", "AS__PRS_SOMEIPSD_00566", "SOMEIP_ETS_162"],
        test_type="fault-injection",
        derivation_technique="error-guessing",
    )
    def test_ets_162_subscribe_endpoint_ip_is_dut_own(
        self,
        someipd_dut: TargetStack,
        sender: socket.socket,
        tc8_host_ip: str,
        tc8_dut_ip: str,
    ) -> None:
        """SubscribeEventgroup whose endpoint IP is the DUT's own address, not the subscriber's.

        The DUT must send an explicit NAck.
        """
        assert someipd_dut.poll() is None, "DUT is not running before injection"
        send_sd_subscribe_endpoint_ip(
            sender, (tc8_dut_ip, SD_PORT), SERVICE_ID, INSTANCE_ID, EVENTGROUP_ID, tc8_dut_ip, PROBE_SUBSCRIBER_PORT
        )
        assert_nack_received(sender, SERVICE_ID, INSTANCE_ID, EVENTGROUP_ID)
        verify_dut_alive(tc8_host_ip)

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_robustness", "AS__PRS_SOMEIPSD_00393"],
        partially_verifies=["AS__PRS_SOMEIPSD_00394", "AS__PRS_SOMEIPSD_00566", "SOMEIP_ETS_164"],
        test_type="fault-injection",
        derivation_technique="error-guessing",
    )
    def test_ets_164_subscribe_endpoint_ip_specific_invalid(
        self,
        someipd_dut: TargetStack,
        sender: socket.socket,
        tc8_host_ip: str,
        tc8_dut_ip: str,
    ) -> None:
        """SubscribeEventgroup with endpoint IP 111.111.111.111.

        The DUT must send an explicit NAck.
        """
        assert someipd_dut.poll() is None, "DUT is not running before injection"
        send_sd_subscribe_endpoint_ip(
            sender,
            (tc8_dut_ip, SD_PORT),
            SERVICE_ID,
            INSTANCE_ID,
            EVENTGROUP_ID,
            "111.111.111.111",
            PROBE_SUBSCRIBER_PORT,
        )
        assert_nack_received(sender, SERVICE_ID, INSTANCE_ID, EVENTGROUP_ID)
        verify_dut_alive(tc8_host_ip)

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_robustness", "AS__PRS_SOMEIPSD_00393"],
        partially_verifies=[
            "AS__PRS_SOMEIPSD_00130",
            "AS__PRS_SOMEIPSD_00234",
            "AS__PRS_SOMEIPSD_00394",
            "AS__PRS_SOMEIPSD_00566",
            "SOMEIP_ETS_119",
        ],
        test_type="fault-injection",
        derivation_technique="error-guessing",
    )
    def test_ets_119_subscribe_unknown_l4proto(
        self,
        someipd_dut: TargetStack,
        sender: socket.socket,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        tc8_tester_ip: str,
    ) -> None:
        """SubscribeEventgroup with L4 protocol byte = 0x00 (neither UDP nor TCP).

        The subscribe has no usable endpoint option. The DUT must send an explicit NAck.
        """
        assert someipd_dut.poll() is None, "DUT is not running before injection"
        send_sd_subscribe_wrong_l4proto(
            sender,
            (tc8_dut_ip, SD_PORT),
            SERVICE_ID,
            INSTANCE_ID,
            EVENTGROUP_ID,
            tc8_tester_ip,
            PROBE_SUBSCRIBER_PORT,
            l4proto=0x00,
        )
        assert_nack_received(sender, SERVICE_ID, INSTANCE_ID, EVENTGROUP_ID)
        verify_dut_alive(tc8_host_ip)

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_robustness", "AS__PRS_SOMEIPSD_00393"],
        partially_verifies=[
            "AS__PRS_SOMEIPSD_00129",
            "AS__PRS_SOMEIPSD_00394",
            "AS__PRS_SOMEIPSD_00566",
            "SOMEIP_ETS_140",
        ],
        test_type="fault-injection",
        derivation_technique="error-guessing",
    )
    def test_ets_140_subscribe_unknown_eventgroup_id(
        self,
        someipd_dut: TargetStack,
        sender: socket.socket,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        tc8_tester_ip: str,
    ) -> None:
        """SubscribeEventgroup for the correct service and instance, but with an unknown Eventgroup-ID.

        The DUT must send an explicit NAck.
        """
        assert someipd_dut.poll() is None, "DUT is not running before injection"
        send_sd_subscribe_nonexistent_service(
            sender,
            (tc8_dut_ip, SD_PORT),
            SERVICE_ID,
            INSTANCE_ID,
            _UNKNOWN_EVENTGROUP_ID,
            tc8_tester_ip,
            PROBE_SUBSCRIBER_PORT,
        )
        assert_nack_received(sender, SERVICE_ID, INSTANCE_ID, _UNKNOWN_EVENTGROUP_ID)
        verify_dut_alive(tc8_host_ip)

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_robustness", "AS__PRS_SOMEIPSD_00393"],
        partially_verifies=[
            "AS__PRS_SOMEIPSD_00127",
            "AS__PRS_SOMEIPSD_00394",
            "AS__PRS_SOMEIPSD_00566",
            "SOMEIP_ETS_141",
        ],
        test_type="fault-injection",
        derivation_technique="error-guessing",
    )
    def test_ets_141_subscribe_unknown_instance_id(
        self,
        someipd_dut: TargetStack,
        sender: socket.socket,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        tc8_tester_ip: str,
    ) -> None:
        """SubscribeEventgroup for the correct service_id, but with an unknown instance_id.

        The DUT must send an explicit NAck.
        """
        assert someipd_dut.poll() is None, "DUT is not running before injection"
        send_sd_subscribe_nonexistent_service(
            sender,
            (tc8_dut_ip, SD_PORT),
            SERVICE_ID,
            _UNKNOWN_INSTANCE_ID,
            EVENTGROUP_ID,
            tc8_tester_ip,
            PROBE_SUBSCRIBER_PORT,
        )
        assert_nack_received(sender, SERVICE_ID, _UNKNOWN_INSTANCE_ID, EVENTGROUP_ID)
        verify_dut_alive(tc8_host_ip)

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_robustness", "AS__PRS_SOMEIPSD_00393"],
        partially_verifies=[
            "AS__PRS_SOMEIPSD_00128",
            "AS__PRS_SOMEIPSD_00394",
            "AS__PRS_SOMEIPSD_00566",
            "SOMEIP_ETS_142",
        ],
        test_type="fault-injection",
        derivation_technique="error-guessing",
    )
    def test_ets_142_subscribe_unknown_major_version(
        self,
        someipd_dut: TargetStack,
        sender: socket.socket,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        tc8_tester_ip: str,
    ) -> None:
        """SubscribeEventgroup for the correct service, instance and eventgroup, but with an unknown Major Version.

        The DUT must send an explicit NAck.
        """
        assert someipd_dut.poll() is None, "DUT is not running before injection"
        send_sd_subscribe_wrong_major_version(
            sender,
            (tc8_dut_ip, SD_PORT),
            SERVICE_ID,
            INSTANCE_ID,
            EVENTGROUP_ID,
            tc8_tester_ip,
            PROBE_SUBSCRIBER_PORT,
        )
        assert_nack_received(sender, SERVICE_ID, INSTANCE_ID, EVENTGROUP_ID)
        verify_dut_alive(tc8_host_ip)

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_robustness", "AS__PRS_SOMEIPSD_00393"],
        partially_verifies=[
            "AS__PRS_SOMEIPSD_00126",
            "AS__PRS_SOMEIPSD_00394",
            "AS__PRS_SOMEIPSD_00566",
            "SOMEIP_ETS_143",
        ],
        test_type="fault-injection",
        derivation_technique="error-guessing",
    )
    def test_ets_143_subscribe_unknown_service_id(
        self,
        someipd_dut: TargetStack,
        sender: socket.socket,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        tc8_tester_ip: str,
    ) -> None:
        """SubscribeEventgroup for an unknown ServiceID. The instance and eventgroup are correct.

        The DUT must send an explicit NAck.
        """
        assert someipd_dut.poll() is None, "DUT is not running before injection"
        send_sd_subscribe_nonexistent_service(
            sender,
            (tc8_dut_ip, SD_PORT),
            UNKNOWN_SERVICE_ID,
            INSTANCE_ID,
            EVENTGROUP_ID,
            tc8_tester_ip,
            PROBE_SUBSCRIBER_PORT,
        )
        assert_nack_received(sender, UNKNOWN_SERVICE_ID, INSTANCE_ID, EVENTGROUP_ID)
        verify_dut_alive(tc8_host_ip)

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_robustness", "AS__PRS_SOMEIPSD_00390"],
        partially_verifies=["AS__PRS_SOMEIPSD_00391", "SOMEIP_ETS_144"],
        test_type="fault-injection",
        derivation_technique="error-guessing",
    )
    def test_ets_144_subscribe_reserved_option_type(
        self,
        someipd_dut: TargetStack,
        sender: socket.socket,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        tc8_tester_ip: str,
    ) -> None:
        """SubscribeEventgroup with a valid IPv4EndpointOption whose reserved fields are non-zero.

        The option is correct in all other fields. Thus the DUT ignores the reserved bits and sends an Ack.
        """
        assert someipd_dut.poll() is None, "DUT is not running before injection"
        send_sd_subscribe_endpoint_reserved_bits_set(
            sender,
            (tc8_dut_ip, SD_PORT),
            SERVICE_ID,
            INSTANCE_ID,
            EVENTGROUP_ID,
            tc8_tester_ip,
            PROBE_SUBSCRIBER_PORT,
        )
        assert_ack_received(sender, SERVICE_ID, INSTANCE_ID, EVENTGROUP_ID)
        verify_dut_alive(tc8_host_ip)


class TestSDStopSubscribeRejected:
    """A StopSubscribeEventgroup (TTL=0) that the DUT rejects gets no Ack and no NAck.

    Each test injects a fault that gets a NAck for the same entry with TTL>0.
    The faults are an unknown major version, an unknown service, and too many option references.
    """

    @add_test_properties(
        fully_verifies=["AS__PRS_SOMEIPSD_00128", "AS__PRS_SOMEIPSD_00393"],
        test_type="fault-injection",
        derivation_technique="error-guessing",
    )
    def test_stop_subscribe_unknown_major_version_gets_no_reply(
        self,
        someipd_dut: TargetStack,
        sender: socket.socket,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        tc8_tester_ip: str,
    ) -> None:
        """A StopSubscribe with a major version the DUT does not offer is ignored."""
        assert someipd_dut.poll() is None, "DUT is not running before injection"
        send_sd_subscribe_wrong_major_version(
            sender,
            (tc8_dut_ip, SD_PORT),
            SERVICE_ID,
            INSTANCE_ID,
            EVENTGROUP_ID,
            tc8_tester_ip,
            PROBE_SUBSCRIBER_PORT,
            ttl=0,
        )
        assert_no_subscribe_reply(sender, SERVICE_ID, INSTANCE_ID, EVENTGROUP_ID)
        verify_dut_alive(tc8_host_ip)

    @add_test_properties(
        fully_verifies=["AS__PRS_SOMEIPSD_00126", "AS__PRS_SOMEIPSD_00393"],
        test_type="fault-injection",
        derivation_technique="error-guessing",
    )
    def test_stop_subscribe_unknown_service_id_gets_no_reply(
        self,
        someipd_dut: TargetStack,
        sender: socket.socket,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        tc8_tester_ip: str,
    ) -> None:
        """A StopSubscribe for a service the DUT does not offer is ignored."""
        assert someipd_dut.poll() is None, "DUT is not running before injection"
        send_sd_subscribe_nonexistent_service(
            sender,
            (tc8_dut_ip, SD_PORT),
            UNKNOWN_SERVICE_ID,
            INSTANCE_ID,
            EVENTGROUP_ID,
            tc8_tester_ip,
            PROBE_SUBSCRIBER_PORT,
            ttl=0,
        )
        assert_no_subscribe_reply(sender, UNKNOWN_SERVICE_ID, INSTANCE_ID, EVENTGROUP_ID)
        verify_dut_alive(tc8_host_ip)

    @add_test_properties(
        fully_verifies=["AS__PRS_SOMEIPSD_00234", "AS__PRS_SOMEIPSD_00393"],
        test_type="fault-injection",
        derivation_technique="error-guessing",
    )
    def test_stop_subscribe_with_invalid_options_gets_no_reply(
        self,
        someipd_dut: TargetStack,
        sender: socket.socket,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        tc8_tester_ip: str,
    ) -> None:
        """A StopSubscribe referencing more options than the message carries is ignored."""
        assert someipd_dut.poll() is None, "DUT is not running before injection"
        send_sd_entry_refs_more_options(
            sender,
            (tc8_dut_ip, SD_PORT),
            SERVICE_ID,
            INSTANCE_ID,
            EVENTGROUP_ID,
            tc8_tester_ip,
            PROBE_SUBSCRIBER_PORT,
            ttl=0,
        )
        assert_no_subscribe_reply(sender, SERVICE_ID, INSTANCE_ID, EVENTGROUP_ID)
        verify_dut_alive(tc8_host_ip)


class TestSDMessageFramingErrors:
    """Tests for SOME/IP length field errors, also a length field past the end of the datagram."""

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_robustness"],
        partially_verifies=["AS__PRS_SOMEIP_00140", "AS__PRS_SOMEIPSD_00125", "SOMEIP_ETS_153"],
        test_type="fault-injection",
        derivation_technique="error-guessing",
    )
    def test_ets_153_someip_length_too_small(
        self,
        someipd_dut: TargetStack,
        sender: socket.socket,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        tc8_tester_ip: str,
    ) -> None:
        """SubscribeEventgroup whose SOME/IP length field (8) is smaller than the payload.

        The DUT must send a NAck or ignore the request. Only an accepting Ack is a failure.
        """
        assert someipd_dut.poll() is None, "DUT is not running before injection"
        send_sd_subscribe_wrong_someip_length(
            sender,
            (tc8_dut_ip, SD_PORT),
            SERVICE_ID,
            INSTANCE_ID,
            EVENTGROUP_ID,
            tc8_tester_ip,
            PROBE_SUBSCRIBER_PORT,
            length_override=8,
        )
        assert_no_subscribe_ack(sender, SERVICE_ID, INSTANCE_ID, EVENTGROUP_ID)
        verify_dut_alive(tc8_host_ip)

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_robustness"],
        partially_verifies=["AS__PRS_SOMEIP_00910", "AS__PRS_SOMEIP_00140"],
        test_type="fault-injection",
        derivation_technique="error-guessing",
    )
    def test_someip_length_too_large(
        self,
        someipd_dut: TargetStack,
        sender: socket.socket,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        tc8_tester_ip: str,
    ) -> None:
        """A valid SubscribeEventgroup with a SOME/IP length field (0x1000) larger than the datagram gets no reply.

        The DUT must discard the message as malformed. Thus the DUT sends no Ack and no NAck.
        """
        assert someipd_dut.poll() is None, "DUT is not running before injection"
        send_sd_subscribe_wrong_someip_length(
            sender,
            (tc8_dut_ip, SD_PORT),
            SERVICE_ID,
            INSTANCE_ID,
            EVENTGROUP_ID,
            tc8_tester_ip,
            PROBE_SUBSCRIBER_PORT,
            length_override=0x1000,
        )
        assert_no_subscribe_reply(sender, SERVICE_ID, INSTANCE_ID, EVENTGROUP_ID)
        verify_dut_alive(tc8_host_ip)


class TestSDWrongServiceIdFraming:
    """Tests for a SubscribeEventgroup with a SOME/IP Message ID that is not the SD Message ID."""

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_robustness"],
        partially_verifies=["AS__PRS_SOMEIPSD_00151", "AS__PRS_SOMEIPSD_00251"],
        test_type="fault-injection",
        derivation_technique="error-guessing",
    )
    def test_wrong_someip_service_id_ignored(
        self,
        someipd_dut: TargetStack,
        sender: socket.socket,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        tc8_tester_ip: str,
    ) -> None:
        """A SubscribeEventgroup with SOME/IP service_id 0x1234 (not the SD value 0xFFFF) gets no SD reply.

        This test asserts the current DUT behavior: someipd drops a datagram that has a non-SD Message ID.
        The OA test expects a SubscribeEventgroupNAck. test_ets_178_wrong_message_id_nacked asserts it.
        """
        assert someipd_dut.poll() is None, "DUT is not running before injection"
        send_sd_wrong_someip_message_id(
            sender,
            (tc8_dut_ip, SD_PORT),
            SERVICE_ID,
            INSTANCE_ID,
            EVENTGROUP_ID,
            tc8_tester_ip,
            PROBE_SUBSCRIBER_PORT,
        )
        assert_no_sd_reply(sender)
        verify_dut_alive(tc8_host_ip)

    @pytest.mark.xfail(
        strict=True,
        raises=AssertionError,
        reason="someipd drops a datagram that has a non-SD Message ID before it decodes the entries, so it "
        "sends no SubscribeEventgroupNAck",
    )
    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_robustness"],
        partially_verifies=["SOMEIP_ETS_178"],
        test_type="fault-injection",
        derivation_technique="error-guessing",
    )
    def test_ets_178_wrong_message_id_nacked(
        self,
        someipd_dut: TargetStack,
        sender: socket.socket,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        tc8_tester_ip: str,
    ) -> None:
        """A SubscribeEventgroup with a non-SD SOME/IP Message ID gets a SubscribeEventgroupNAck.

        test_wrong_someip_service_id_ignored asserts the current DUT behavior, which is no reply.
        """
        assert someipd_dut.poll() is None, "DUT is not running before injection"
        send_sd_wrong_someip_message_id(
            sender,
            (tc8_dut_ip, SD_PORT),
            SERVICE_ID,
            INSTANCE_ID,
            EVENTGROUP_ID,
            tc8_tester_ip,
            PROBE_SUBSCRIBER_PORT,
        )
        assert_nack_received(sender, SERVICE_ID, INSTANCE_ID, EVENTGROUP_ID)
        verify_dut_alive(tc8_host_ip)
