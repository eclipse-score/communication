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
"""TC8 tests for the SD message format and the SD options of the someipd DUT.

The tests check the byte values of the SD OfferService messages and the SubscribeEventgroupAck
responses. Most tests only examine the normal SD traffic of the DUT.
Two classes have their own DUT: one test stops the DUT to check the offer TTL,
and one test sends many Subscribes until the Session ID goes from 0xFFFF back to 1.
"""

import ipaddress
import socket
import time

import pytest
from attribute_plugin import add_test_properties
from scapy.contrib.automotive.someip import SDOption_IP4_EndPoint
from someip_itf.codec import (
    SD_INTERFACE_VERSION,
    L4Proto,
    MessageType,
    ReturnCode,
    SdEntry,
    SdEntryType,
)
from someip_itf.tc8.constants import (
    DUT_UNRELIABLE_PORT,
    EVENTGROUP_ID,
    INSTANCE_ID,
    MAJOR_VERSION,
    METHOD_ID,
    MINOR_VERSION,
    MULTICAST_EVENT_PORT,
    MULTICAST_EVENTGROUP_ID,
    PROBE_SUBSCRIBER_PORT,
    SD_MULTICAST_ADDR,
    SD_PORT,
    SERVICE_ID,
    UNKNOWN_SERVICE_ID,
)
from someip_itf.tc8.dut_lifecycle import TargetStack
from someip_itf.tc8.events import subscribe_and_wait_ack
from someip_itf.tc8.message_builder import build_request
from someip_itf.tc8.probes import (
    SUBSCRIBE_TTL,
    capture_multicast_entry,
    capture_raw_sd_offer,
    capture_subscribe_ack,
    capture_subscribe_ack_with_options,
    encoded_subscribe_entry,
    multicast_option,
    udp_endpoint_option,
)
from someip_itf.tc8.sd import collect_sd_messages_from_socket, create_udp_socket
from someip_itf.tc8.sd_sender import (
    capture_some_ip_messages_with_addr,
    open_sender_socket,
    send_subscribe_eventgroup,
)

#: SERVICE-OFFER-TIME-TO-LIVE in the SD server configuration of the DUT.
_OFFER_TTL: int = 10
#: Reserved OfferService TTL value. It means "valid until the next reboot", not a number of seconds.
#: A test that waits for the end of a captured TTL cannot use this value.
_TTL_INFINITE: int = 0xFFFFFF
#: A second, different TTL. It shows that the Ack repeats the *requested* TTL, not a fixed
#: configuration value that is equal to `SUBSCRIBE_TTL`, which all other subscribes in this module use.
_ALT_SUBSCRIBE_TTL: int = 7
#: Notification ID on the wire = 0x8000 | the declared event ID (0x0777).
_EVENT_NOTIFIER: int = 0x8777


class TestSdHeaderFieldsOfferService:
    """SOME/IP header fields of an OfferService SD message."""

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_format_fields"],
        partially_verifies=["SOMEIPSRV_FORMAT_01"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_format_01_client_id_is_zero(self, someipd_dut: TargetStack, tc8_host_ip: str) -> None:
        """The client_id in the SOME/IP SD header must be 0x0000."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        someip_msg, _, _ = capture_raw_sd_offer(tc8_host_ip)

        assert someip_msg.client_id == 0x0000, f"client_id must be 0x0000; got 0x{someip_msg.client_id:04x}"

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_format_fields"],
        partially_verifies=["SOMEIPSRV_FORMAT_02"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_format_02_session_id_is_nonzero(self, someipd_dut: TargetStack, tc8_host_ip: str) -> None:
        """The SD session_id must not be zero.

        The session_id starts at 0x0001 and increases by one with each SD message. The value
        0x0000 is reserved. The DUT can already have sent SD messages during other tests,
        thus the captured value can be more than 1. But it must not be 0.

        The test cannot check that the value is 0x0001 directly after the SD start, because other
        tests share the DUT. A different test with a restarted DUT checks that case.
        """
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        someip_msg, _, _ = capture_raw_sd_offer(tc8_host_ip, timeout_secs=5.0)

        assert someip_msg.session_id != 0x0000, "session_id must never be 0x0000 (reserved)"

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_format_fields"],
        partially_verifies=["SOMEIPSRV_FORMAT_03"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_format_03_protocol_version_is_one(self, someipd_dut: TargetStack, tc8_host_ip: str) -> None:
        """The SOME/IP protocol_version in the SD message header must be 1."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        someip_msg, _, _ = capture_raw_sd_offer(tc8_host_ip)

        assert someip_msg.protocol_version == 1, (
            f"protocol_version must be 0x01; got 0x{someip_msg.protocol_version:02x}"
        )

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_format_fields"],
        partially_verifies=["SOMEIPSRV_FORMAT_04"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_format_04_interface_version_is_one(self, someipd_dut: TargetStack, tc8_host_ip: str) -> None:
        """The SOME/IP SD interface_version must be 0x01."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        someip_msg, _, _ = capture_raw_sd_offer(tc8_host_ip)

        assert someip_msg.interface_version == SD_INTERFACE_VERSION, (
            f"interface_version must be {SD_INTERFACE_VERSION}; got {someip_msg.interface_version}"
        )

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_format_fields"],
        partially_verifies=["SOMEIPSRV_FORMAT_05"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_format_05_message_type_is_notification(self, someipd_dut: TargetStack, tc8_host_ip: str) -> None:
        """The SOME/IP SD message_type must be NOTIFICATION (0x02)."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        someip_msg, _, _ = capture_raw_sd_offer(tc8_host_ip)

        assert someip_msg.message_type == MessageType.NOTIFICATION, (
            f"message_type must be NOTIFICATION (0x{MessageType.NOTIFICATION:02x}); got 0x{someip_msg.message_type:02x}"
        )

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_format_fields"],
        partially_verifies=["SOMEIPSRV_FORMAT_06"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_format_06_return_code_is_e_ok(self, someipd_dut: TargetStack, tc8_host_ip: str) -> None:
        """The SOME/IP SD return_code must be E_OK (0x00)."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        someip_msg, _, _ = capture_raw_sd_offer(tc8_host_ip)

        assert someip_msg.return_code == ReturnCode.E_OK, (
            f"return_code must be E_OK (0x{ReturnCode.E_OK:02x}); got 0x{someip_msg.return_code:02x}"
        )

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_format_fields"],
        partially_verifies=["SOMEIPSRV_FORMAT_08"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_format_08_unicast_flag_set(self, someipd_dut: TargetStack, tc8_host_ip: str) -> None:
        """In an OfferService, the Unicast flag (bit 6) of the SD Flags byte must be set."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        _, sd_hdr, _ = capture_raw_sd_offer(tc8_host_ip)

        assert sd_hdr.flag_unicast, f"SD Flags Unicast flag (bit 6) must be set; flag_unicast={sd_hdr.flag_unicast}"

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_format_fields"],
        partially_verifies=["SOMEIPSRV_FORMAT_09"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_format_09_sd_flags_reserved_bits_are_zero(self, someipd_dut: TargetStack, tc8_host_ip: str) -> None:
        """The reserved bits (5-0) of the SD Flags byte must be 0."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        _, sd_hdr, _ = capture_raw_sd_offer(tc8_host_ip)

        # flags_unknown has bits 5-0 (the reserved bits).
        assert sd_hdr.flags_unknown == 0, (
            f"SD Flags reserved bits (5-0) must be 0; flags_unknown=0x{sd_hdr.flags_unknown:02x}"
        )

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_format_fields"],
        partially_verifies=["SOMEIPSRV_FORMAT_10"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_format_10_sd_entry_reserved_bytes_are_zero(self, someipd_dut: TargetStack, tc8_host_ip: str) -> None:
        """The 3-byte Reserved field of the SD header (bytes [1-3], after Flags) must be 0."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        _, sd_hdr, _ = capture_raw_sd_offer(tc8_host_ip)

        assert sd_hdr.reserved == 0, (
            f"SD header Reserved field (bytes [1-3] of the SD payload) must be 0x000000; got 0x{sd_hdr.reserved:06x}"
        )


class TestSdOfferEntryFields:
    """Fields of the OfferService SD entry."""

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_format_fields"],
        partially_verifies=["SOMEIPSRV_FORMAT_11"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_format_11_entry_is_16_bytes(self, someipd_dut: TargetStack, tc8_host_ip: str) -> None:
        """The Type byte of the OfferService entry is 0x01, and the Entries Array Length is 16.

        The decoder always uses 16 bytes for an entry, so the length of a decoded entry is always correct.
        The Entries Array Length check rejects an offer message with more than one entry.
        If the Type byte is not one of the four known entry types, the decoder cannot read the datagram.
        Then the test fails with a capture timeout, not on the Type check.
        """
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        # The capture uses the service and the instance, not the type. Thus the capture does not
        # decide the result of the Type byte check below.
        _, sd_hdr, entry = capture_multicast_entry(
            tc8_host_ip, lambda e: e.service_id == SERVICE_ID and e.instance_id == INSTANCE_ID, timeout_secs=5.0
        )

        assert entry.sd_type == 0x01, f"OfferService entry Type byte must be 0x01; got 0x{entry.sd_type:02x}"
        assert sd_hdr.entries_length == 16, f"Entries Array Length must be 16 (1 entry); got {sd_hdr.entries_length}"

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_format_fields"],
        partially_verifies=["SOMEIPSRV_FORMAT_12"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_format_12_first_option_run_index_is_zero(self, someipd_dut: TargetStack, tc8_host_ip: str) -> None:
        """The index of the first option run of the OfferService must be 0."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        _, _, raw_entry = capture_raw_sd_offer(tc8_host_ip)

        assert raw_entry.option_index_1 == 0, (
            f"option_index_1 (byte [1]) must be 0; got 0x{raw_entry.option_index_1:02x}"
        )

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_format_fields"],
        partially_verifies=["SOMEIPSRV_FORMAT_13"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_format_13_num_options_matches_options_list(self, someipd_dut: TargetStack, tc8_host_ip: str) -> None:
        """An OfferService entry must refer to at least one option.

        `num_options_1` is always equal to `len(options_1)`, because the decoder uses that counter
        to get `options_1`. Thus the test checks only the lower limit.
        """
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        _, _, raw_entry = capture_raw_sd_offer(tc8_host_ip)

        assert raw_entry.num_options_1 >= 1, (
            f"OfferService entry must reference at least one option (num_options_1 >= 1); "
            f"got num_options_1={raw_entry.num_options_1}"
        )

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_format_fields"],
        partially_verifies=["SOMEIPSRV_FORMAT_14"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_format_14_entry_type_is_offer(self, someipd_dut: TargetStack, tc8_host_ip: str) -> None:
        """The Service ID of the OfferService entry must be equal to the configured service.

        This test checks the Service ID field, not the entry Type byte. The other tests use
        the Service ID only to select the entry in `capture_raw_sd_offer`.
        """
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        _, _, entry = capture_raw_sd_offer(tc8_host_ip)

        assert entry.service_id == SERVICE_ID, (
            f"OfferService entry Service ID must be 0x{SERVICE_ID:04x}; got 0x{entry.service_id:04x}"
        )

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_format_fields"],
        partially_verifies=["SOMEIPSRV_FORMAT_15"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_format_15_instance_id_matches_config(self, someipd_dut: TargetStack, tc8_host_ip: str) -> None:
        """The OfferService instance_id must be equal to the configured value."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        _, _, entry = capture_raw_sd_offer(tc8_host_ip)

        assert entry.instance_id == INSTANCE_ID, (
            f"instance_id must be 0x{INSTANCE_ID:04x}; got 0x{entry.instance_id:04x}"
        )

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_format_fields"],
        partially_verifies=["SOMEIPSRV_FORMAT_16"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_format_16_major_version_matches_config(self, someipd_dut: TargetStack, tc8_host_ip: str) -> None:
        """The OfferService major_version must be equal to the configured value."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        _, _, entry = capture_raw_sd_offer(tc8_host_ip)

        assert entry.major_version == MAJOR_VERSION, (
            f"major_version must be 0x{MAJOR_VERSION:02x}; got 0x{entry.major_version:02x}"
        )

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_format_fields"],
        partially_verifies=["SOMEIPSRV_FORMAT_17"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_format_17_ttl_is_nonzero(self, someipd_dut: TargetStack, tc8_host_ip: str) -> None:
        """The TTL of the OfferService entry (3-byte big-endian, bytes[9-11]) must be equal to the configured
        SERVICE-OFFER-TIME-TO-LIVE."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        _, _, entry = capture_raw_sd_offer(tc8_host_ip)

        assert entry.ttl == _OFFER_TTL, f"OfferService entry TTL (bytes[9-11]) must be {_OFFER_TTL}; got {entry.ttl}"

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_format_fields"],
        partially_verifies=["SOMEIPSRV_FORMAT_18"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_format_18_minor_version_matches_config(self, someipd_dut: TargetStack, tc8_host_ip: str) -> None:
        """The OfferService minor_version must be equal to the configured value."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        _, _, entry = capture_raw_sd_offer(tc8_host_ip)

        assert entry.service_minor_version == MINOR_VERSION, (
            f"minor_version must be 0x{MINOR_VERSION:08x}; got 0x{entry.service_minor_version:08x}"
        )


class TestSdHeaderFieldsSubscribeAck:
    """Fields of the SubscribeEventgroupAck entry."""

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_format_fields"],
        partially_verifies=["SOMEIPSRV_FORMAT_19"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_format_19_ack_entry_type_is_subscribe_ack(
        self, someipd_dut: TargetStack, tc8_host_ip: str, tc8_dut_ip: str, tc8_tester_ip: str
    ) -> None:
        """The SD entry type of the SubscribeEventgroupAck must be 0x07."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        ack = capture_subscribe_ack(tc8_dut_ip, tc8_tester_ip, EVENTGROUP_ID)

        assert ack.sd_type == SdEntryType.SubscribeAck, f"sd_type must be SubscribeAck (0x07); got {ack.sd_type!r}"

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_format_fields"],
        partially_verifies=["SOMEIPSRV_FORMAT_20"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_format_20_ack_entry_is_16_bytes(
        self, someipd_dut: TargetStack, tc8_host_ip: str, tc8_dut_ip: str, tc8_tester_ip: str
    ) -> None:
        """The Type byte of the SubscribeAck entry is 0x07, and the Entries Array Length is 16.

        The decoder always uses 16 bytes for an entry, so the length of a decoded entry is always correct.
        The Entries Array Length check rejects an Ack message with more than one entry.
        If the Type byte is not one of the four known entry types, the decoder cannot read the datagram.
        Then the test fails with a capture timeout, not on the Type check.
        """
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        sd_hdr, ack = capture_subscribe_ack_with_options(tc8_dut_ip, tc8_tester_ip, EVENTGROUP_ID, match_type=False)

        assert ack.sd_type == 0x07, f"SubscribeAck entry Type byte must be 0x07; got 0x{ack.sd_type:02x}"
        assert sd_hdr.entries_length == 16, f"Entries Array Length must be 16 (1 entry); got {sd_hdr.entries_length}"

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_format_fields"],
        partially_verifies=["SOMEIPSRV_FORMAT_21"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_format_21_ack_option_run_index_is_zero_when_no_options(
        self, someipd_dut: TargetStack, tc8_host_ip: str, tc8_dut_ip: str, tc8_tester_ip: str
    ) -> None:
        """The SubscribeAck option_index_1 must be 0.

        The Ack is the only entry in its SD message. Thus its first option run always starts at
        index 0 of the options array, also when the Ack has no options.
        """
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        _, ack = capture_subscribe_ack_with_options(tc8_dut_ip, tc8_tester_ip, EVENTGROUP_ID)

        assert ack.option_index_1 == 0, f"option_index_1 must be 0; got 0x{ack.option_index_1:02x}"

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_format_fields"],
        partially_verifies=["SOMEIPSRV_FORMAT_23"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_format_23_ack_service_id_matches_config(
        self, someipd_dut: TargetStack, tc8_host_ip: str, tc8_dut_ip: str, tc8_tester_ip: str
    ) -> None:
        """The SubscribeAck service_id must be equal to the subscribed service."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        ack = capture_subscribe_ack(tc8_dut_ip, tc8_tester_ip, EVENTGROUP_ID)

        assert ack.service_id == SERVICE_ID, f"service_id must be 0x{SERVICE_ID:04x}; got 0x{ack.service_id:04x}"

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_format_fields"],
        partially_verifies=["SOMEIPSRV_FORMAT_24"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_format_24_ack_instance_id_matches_config(
        self, someipd_dut: TargetStack, tc8_host_ip: str, tc8_dut_ip: str, tc8_tester_ip: str
    ) -> None:
        """The SubscribeAck instance_id must be equal to the subscribed instance."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        ack = capture_subscribe_ack(tc8_dut_ip, tc8_tester_ip, EVENTGROUP_ID)

        assert ack.instance_id == INSTANCE_ID, f"instance_id must be 0x{INSTANCE_ID:04x}; got 0x{ack.instance_id:04x}"

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_format_fields"],
        partially_verifies=["SOMEIPSRV_FORMAT_25"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_format_25_ack_major_version_matches_config(
        self, someipd_dut: TargetStack, tc8_host_ip: str, tc8_dut_ip: str, tc8_tester_ip: str
    ) -> None:
        """The SubscribeAck major_version must be equal to the service definition."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        ack = capture_subscribe_ack(tc8_dut_ip, tc8_tester_ip, EVENTGROUP_ID)

        assert ack.major_version == MAJOR_VERSION, (
            f"major_version must be 0x{MAJOR_VERSION:02x}; got 0x{ack.major_version:02x}"
        )

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_format_fields"],
        partially_verifies=["SOMEIPSRV_FORMAT_26"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_format_26_ack_ttl_is_nonzero(
        self, someipd_dut: TargetStack, tc8_host_ip: str, tc8_dut_ip: str, tc8_tester_ip: str
    ) -> None:
        """The SubscribeEventgroupAck TTL must be equal to the TTL of the subscribe (TTL=0 means NAck)."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        ack = capture_subscribe_ack(tc8_dut_ip, tc8_tester_ip, EVENTGROUP_ID)

        assert ack.ttl == SUBSCRIBE_TTL, (
            f"SubscribeAck TTL must echo the requested TTL ({SUBSCRIBE_TTL}); got {ack.ttl}"
        )

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_format_fields"],
        partially_verifies=["SOMEIPSRV_FORMAT_27"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_format_27_ack_reserved_field_is_zero(
        self, someipd_dut: TargetStack, tc8_host_ip: str, tc8_dut_ip: str, tc8_tester_ip: str
    ) -> None:
        """The reserved bits (high 16 bits of minver_or_counter) of the SubscribeAck must be 0.

        In the 16-byte SubscribeEventgroupAck entry, bytes [12-13] are a reserved counter (must be 0).
        Bytes [14-15] are the eventgroup_id. `minver_or_counter` is `(reserved << 16) | eventgroup_id`.
        """
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        ack = capture_subscribe_ack(tc8_dut_ip, tc8_tester_ip, EVENTGROUP_ID)

        reserved_high = (ack.minver_or_counter >> 16) & 0xFFFF
        assert reserved_high == 0, (
            f"High 16 bits of SubscribeAck minver_or_counter must be 0; got 0x{reserved_high:04x} "
            f"(minver_or_counter=0x{ack.minver_or_counter:08x})"
        )

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_format_fields"],
        partially_verifies=["SOMEIPSRV_FORMAT_28"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_format_28_ack_eventgroup_id_matches_subscribe(
        self, someipd_dut: TargetStack, tc8_host_ip: str, tc8_dut_ip: str, tc8_tester_ip: str
    ) -> None:
        """The SubscribeAck eventgroup_id must be equal to the subscribed eventgroup."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        ack = capture_subscribe_ack(tc8_dut_ip, tc8_tester_ip, EVENTGROUP_ID)

        assert (ack.minver_or_counter & 0xFFFF) == EVENTGROUP_ID, (
            f"eventgroup_id in SubscribeAck must be 0x{EVENTGROUP_ID:04x}; got 0x{ack.minver_or_counter & 0xFFFF:04x}"
        )

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_format_fields"],
        partially_verifies=["SOMEIPSRV_SD_MESSAGE_13"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_sd_message_13_ack_echoes_requested_ttl(
        self, someipd_dut: TargetStack, tc8_host_ip: str, tc8_dut_ip: str, tc8_tester_ip: str
    ) -> None:
        """The SubscribeEventgroupAck must repeat the *requested* TTL, not a fixed configuration value.

        All other subscribes in this module request `SUBSCRIBE_TTL`. A DUT that always sends its
        configured TTL can thus pass the other TTL check. This test requests `_ALT_SUBSCRIBE_TTL`
        to show that the Ack repeats the TTL of this Subscribe.
        """
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        ack = capture_subscribe_ack(tc8_dut_ip, tc8_tester_ip, EVENTGROUP_ID, ttl=_ALT_SUBSCRIBE_TTL)

        assert ack.ttl == _ALT_SUBSCRIBE_TTL, (
            f"SubscribeAck TTL must echo the requested TTL ({_ALT_SUBSCRIBE_TTL}); got {ack.ttl}"
        )


class TestSdOptionsEndpoint:
    """Fields of the IPv4EndpointOption in the OfferService."""

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_options_fields"],
        partially_verifies=["SOMEIPSRV_OPTIONS_01"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_options_01_endpoint_option_length_is_nine(self, someipd_dut: TargetStack, tc8_host_ip: str) -> None:
        """The Length field of the IPv4EndpointOption must be 0x0009."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        _, _, entry = capture_raw_sd_offer(tc8_host_ip)

        length_field = udp_endpoint_option(entry, "Endpoint option length").len
        assert length_field == 0x0009, f"IPv4EndpointOption length field must be 0x0009; got 0x{length_field:04x}"

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_options_fields"],
        partially_verifies=["SOMEIPSRV_OPTIONS_02"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_options_02_endpoint_option_type_is_0x04(self, someipd_dut: TargetStack, tc8_host_ip: str) -> None:
        """The type byte of the IPv4EndpointOption must be 0x04."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        _, _, entry = capture_raw_sd_offer(tc8_host_ip)

        # Select the option by its content, not by its class. Thus the selection does not decide the Type byte check below.
        udp_opts = [o for o in entry.options_1 + entry.options_2 if getattr(o, "l4_proto", None) == L4Proto.UDP]
        assert udp_opts, "No option with L4-Protocol UDP found in OfferService entry"
        type_byte = udp_opts[0].type
        assert type_byte == 0x04, f"IPv4EndpointOption type byte must be 0x04; got 0x{type_byte:02x}"

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_options_fields"],
        partially_verifies=["SOMEIPSRV_OPTIONS_03"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_options_03_endpoint_option_reserved_after_type_is_zero(
        self, someipd_dut: TargetStack, tc8_host_ip: str
    ) -> None:
        """The reserved byte at offset [3] of the IPv4EndpointOption must be 0x00."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        _, _, entry = capture_raw_sd_offer(tc8_host_ip)

        reserved_byte = udp_endpoint_option(entry, "Endpoint option reserved byte [3]").res_hdr
        assert reserved_byte == 0x00, f"IPv4EndpointOption reserved byte [3] must be 0x00; got 0x{reserved_byte:02x}"

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_options_fields"],
        partially_verifies=["SOMEIPSRV_OPTIONS_05"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_options_05_endpoint_option_reserved_before_protocol_is_zero(
        self, someipd_dut: TargetStack, tc8_host_ip: str
    ) -> None:
        """The reserved byte at offset [8] of the IPv4EndpointOption must be 0x00."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        _, _, entry = capture_raw_sd_offer(tc8_host_ip)

        reserved_byte = udp_endpoint_option(entry, "Endpoint option reserved byte [8]").res_tail
        assert reserved_byte == 0x00, f"IPv4EndpointOption reserved byte [8] must be 0x00; got 0x{reserved_byte:02x}"

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_options_fields"],
        partially_verifies=["SOMEIPSRV_OPTIONS_06"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_options_06_endpoint_option_protocol_is_udp(self, someipd_dut: TargetStack, tc8_host_ip: str) -> None:
        """The L4 protocol of the IPv4EndpointOption must be UDP (0x11)."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        _, _, entry = capture_raw_sd_offer(tc8_host_ip)

        opt = udp_endpoint_option(entry, "Endpoint option protocol")
        assert opt.l4_proto == L4Proto.UDP, (
            f"IPv4EndpointOption l4proto must be UDP (0x{L4Proto.UDP:02x}); got {opt.l4_proto!r}"
        )

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_options_fields"],
        partially_verifies=["SOMEIPSRV_OPTIONS_04"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_options_04_endpoint_option_ip_matches_config(
        self, someipd_dut: TargetStack, tc8_host_ip: str, tc8_dut_ip: str
    ) -> None:
        """The address of the IPv4EndpointOption should be equal to the address of the DUT."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        _, _, entry = capture_raw_sd_offer(tc8_host_ip)

        opt = udp_endpoint_option(entry, "Endpoint option address")
        assert opt.addr == tc8_dut_ip, f"IPv4EndpointOption address must be {tc8_dut_ip}; got {opt.addr}"

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_options_fields"],
        partially_verifies=["SOMEIPSRV_OPTIONS_07"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_options_07_endpoint_option_port_matches_config(self, someipd_dut: TargetStack, tc8_host_ip: str) -> None:
        """The port of the IPv4EndpointOption must be equal to the configured UDP port."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        _, _, entry = capture_raw_sd_offer(tc8_host_ip)

        opt = udp_endpoint_option(entry, "Endpoint option port")
        assert opt.port == DUT_UNRELIABLE_PORT, f"IPv4EndpointOption port must be {DUT_UNRELIABLE_PORT}; got {opt.port}"


class TestSdOptionsMulticast:
    """Fields of the IPv4MulticastOption in the SubscribeAck.

    All seven tests are ``xfail``: someipd does not put an IPv4MulticastOption in the SubscribeAck.
    This is a known limitation.

    The tests are skipped on a loopback host_ip. The SD stack of the DUT adds a multicast endpoint option
    only when the subscriber uses a routable interface, not loopback.
    """

    def _mcast_ack_or_skip(self, tc8_host_ip: str, tc8_dut_ip: str, tc8_tester_ip: str, label: str) -> SdEntry:
        if ipaddress.ip_address(tc8_host_ip).is_loopback:
            pytest.skip(
                f"{label}: multicast endpoint option in SubscribeAck requires an interface that is not loopback"
            )
        return capture_subscribe_ack_with_options(tc8_dut_ip, tc8_tester_ip, MULTICAST_EVENTGROUP_ID)[1]

    @pytest.mark.network
    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_options_fields"],
        partially_verifies=["SOMEIPSRV_OPTIONS_08"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_options_08_multicast_option_length_is_nine(
        self, someipd_dut: TargetStack, tc8_host_ip: str, tc8_dut_ip: str, tc8_tester_ip: str
    ) -> None:
        """The Length field of the IPv4MulticastOption must be 0x0009."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        ack = self._mcast_ack_or_skip(tc8_host_ip, tc8_dut_ip, tc8_tester_ip, "Multicast option length")

        length_field = multicast_option(ack, "Multicast option length").len
        assert length_field == 0x0009, f"IPv4MulticastOption length field must be 0x0009; got 0x{length_field:04x}"

    @pytest.mark.network
    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_options_fields"],
        partially_verifies=["SOMEIPSRV_OPTIONS_09"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_options_09_multicast_option_type_is_0x14(
        self, someipd_dut: TargetStack, tc8_host_ip: str, tc8_dut_ip: str, tc8_tester_ip: str
    ) -> None:
        """The type byte of the IPv4MulticastOption must be 0x14 (decimal 20)."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        ack = self._mcast_ack_or_skip(tc8_host_ip, tc8_dut_ip, tc8_tester_ip, "Multicast option type")

        # Select the option by its multicast address, not by its class. Thus the selection does not decide the Type byte check below.
        mcast_opts = [
            o for o in ack.options_1 + ack.options_2 if hasattr(o, "addr") and ipaddress.ip_address(o.addr).is_multicast
        ]
        assert mcast_opts, "No option with a multicast address found in SubscribeAck"
        type_byte = mcast_opts[0].type
        assert type_byte == 0x14, f"IPv4MulticastOption type byte must be 0x14; got 0x{type_byte:02x}"

    @pytest.mark.network
    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_options_fields"],
        partially_verifies=["SOMEIPSRV_OPTIONS_10"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_options_10_multicast_option_reserved_is_zero(
        self, someipd_dut: TargetStack, tc8_host_ip: str, tc8_dut_ip: str, tc8_tester_ip: str
    ) -> None:
        """The reserved byte at offset [3] of the IPv4MulticastOption must be 0x00."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        ack = self._mcast_ack_or_skip(tc8_host_ip, tc8_dut_ip, tc8_tester_ip, "Multicast option reserved byte [3]")

        reserved_byte = multicast_option(ack, "Multicast option reserved byte [3]").res_hdr
        assert reserved_byte == 0x00, f"IPv4MulticastOption reserved byte [3] must be 0x00; got 0x{reserved_byte:02x}"

    @pytest.mark.network
    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_options_fields"],
        partially_verifies=["SOMEIPSRV_OPTIONS_11"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_options_11_multicast_address_matches_config(
        self, someipd_dut: TargetStack, tc8_host_ip: str, tc8_dut_ip: str, tc8_tester_ip: str
    ) -> None:
        """The address of the IPv4MulticastOption must be equal to the configured multicast address."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        ack = self._mcast_ack_or_skip(tc8_host_ip, tc8_dut_ip, tc8_tester_ip, "Multicast option address")

        opt = multicast_option(ack, "Multicast option address")
        assert opt.addr == SD_MULTICAST_ADDR, f"multicast address must be {SD_MULTICAST_ADDR}; got {opt.addr}"

    @pytest.mark.network
    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_options_fields"],
        partially_verifies=["SOMEIPSRV_OPTIONS_12"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_options_12_multicast_option_reserved_before_port_is_zero(
        self, someipd_dut: TargetStack, tc8_host_ip: str, tc8_dut_ip: str, tc8_tester_ip: str
    ) -> None:
        """The reserved byte at offset [8] of the IPv4MulticastOption must be 0x00."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        ack = self._mcast_ack_or_skip(tc8_host_ip, tc8_dut_ip, tc8_tester_ip, "Multicast option reserved byte [8]")

        reserved_byte = multicast_option(ack, "Multicast option reserved byte [8]").res_tail
        assert reserved_byte == 0x00, f"IPv4MulticastOption reserved byte [8] must be 0x00; got 0x{reserved_byte:02x}"

    @pytest.mark.network
    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_options_fields"],
        partially_verifies=["SOMEIPSRV_OPTIONS_13"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_options_13_multicast_option_protocol_is_udp(
        self, someipd_dut: TargetStack, tc8_host_ip: str, tc8_dut_ip: str, tc8_tester_ip: str
    ) -> None:
        """The L4 protocol of the IPv4MulticastOption must be UDP (0x11)."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        ack = self._mcast_ack_or_skip(tc8_host_ip, tc8_dut_ip, tc8_tester_ip, "Multicast option protocol")

        opt = multicast_option(ack, "Multicast option protocol")
        assert opt.l4_proto == L4Proto.UDP, (
            f"IPv4MulticastOption l4proto must be UDP (0x{L4Proto.UDP:02x}); got {opt.l4_proto!r}"
        )

    @pytest.mark.network
    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_options_fields"],
        partially_verifies=["SOMEIPSRV_OPTIONS_14"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_options_14_multicast_port_matches_config(
        self, someipd_dut: TargetStack, tc8_host_ip: str, tc8_dut_ip: str, tc8_tester_ip: str
    ) -> None:
        """The port of the IPv4MulticastOption must be equal to the configured multicast port."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        ack = self._mcast_ack_or_skip(tc8_host_ip, tc8_dut_ip, tc8_tester_ip, "Multicast option port")

        opt = multicast_option(ack, "Multicast option port")
        assert opt.port == MULTICAST_EVENT_PORT, f"multicast port must be {MULTICAST_EVENT_PORT}; got {opt.port}"


class TestSdMessage07TtlLifetime:
    """TTL and the lifetime of a service instance.

    The test captures the TTL of the OfferService entry, stops the service and waits until the TTL expires.
    Then it sends a Request and makes sure that the DUT sends no Response. This test stops the DUT,
    thus it has its own class and its own class-scoped `someipd_dut`. A second `.stop()` in the
    fixture teardown is safe.

    A different test checks the TTL value. This test checks only that the service is not available after the TTL.
    """

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_format_fields"],
        partially_verifies=["SOMEIPSRV_SD_MESSAGE_07"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_sd_message_07_offer_ttl_lifetime(
        self, someipd_dut: TargetStack, tc8_host_ip: str, tc8_dut_ip: str
    ) -> None:
        """After the DUT stops and the TTL of its last offer expires, a Request gets no Response."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        _, _, entry = capture_raw_sd_offer(tc8_host_ip)
        ttl_secs = entry.ttl
        assert ttl_secs != _TTL_INFINITE, (
            "offer TTL is 0xFFFFFF ('valid until reboot'), not a "
            "number of seconds. The test cannot wait for the end of this TTL"
        )

        someipd_dut.stop()
        time.sleep(ttl_secs + 2)  # 2 s margin after the TTL of the offer.

        request_bytes = build_request(
            SERVICE_ID, METHOD_ID, client_id=0x0050, session_id=0x0001, interface_version=MAJOR_VERSION
        )
        sock = create_udp_socket(port=0)
        try:
            sock.sendto(request_bytes, (tc8_dut_ip, DUT_UNRELIABLE_PORT))
            sock.settimeout(3.0)
            with pytest.raises((socket.timeout, OSError)):
                sock.recvfrom(65535)
        finally:
            sock.close()


class TestSdEntryOptionFields:
    """Option run fields of the entries."""

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_format_fields"],
        partially_verifies=["SOMEIPSRV_SD_MESSAGE_08"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_sd_message_08_offer_entry_references_ipv4_endpoint_option(
        self, someipd_dut: TargetStack, tc8_host_ip: str
    ) -> None:
        """An OfferService entry must refer to at least one IPv4 Endpoint Option."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        _, _, entry = capture_raw_sd_offer(tc8_host_ip)

        ipv4_opts = [o for o in entry.options_1 if isinstance(o, SDOption_IP4_EndPoint)]
        assert len(ipv4_opts) >= 1, (
            f"OfferService entry must reference at least one IPv4EndpointOption; got {len(ipv4_opts)}"
        )

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_format_fields"],
        partially_verifies=["SOMEIPSRV_SD_MESSAGE_09"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_sd_message_09_offer_entry_option_port_matches_event_source(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        tc8_tester_ip: str,
    ) -> None:
        """The port of the OfferService UDP Endpoint Option is also the source port of the events.
        The event has Return Code E_OK."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        _, _, entry = capture_raw_sd_offer(tc8_host_ip)
        endpoint_port = udp_endpoint_option(entry, "Event source port").port

        sd_sock = None
        notif_sock = None
        try:
            notif_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            notif_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            notif_sock.bind((tc8_tester_ip, 0))
            notif_port = notif_sock.getsockname()[1]

            sd_sock = subscribe_and_wait_ack(
                tc8_tester_ip,
                tc8_dut_ip,
                SD_PORT,
                SERVICE_ID,
                INSTANCE_ID,
                EVENTGROUP_ID,
                MAJOR_VERSION,
                notif_port=notif_port,
            )

            matches = capture_some_ip_messages_with_addr(
                notif_sock, SERVICE_ID, timeout_secs=5.0, method_id=_EVENT_NOTIFIER
            )
            assert matches, f"No notification for service 0x{SERVICE_ID:04x} received within 5 s"
            notif_msg, notif_addr = matches[0]
            source_port = notif_addr[1]

            assert source_port == endpoint_port, (
                f"notification source port ({source_port}) must match the OfferService "
                f"IPv4EndpointOption port ({endpoint_port})"
            )
            assert notif_msg.return_code == ReturnCode.E_OK, (
                f"notification return_code must be E_OK; got 0x{notif_msg.return_code:02x}"
            )
        finally:
            if sd_sock:
                sd_sock.close()
            if notif_sock:
                notif_sock.close()

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_format_fields"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_sd_message_11_subscribe_entry_type_byte(
        self, someipd_dut: TargetStack, tc8_host_ip: str, tc8_tester_ip: str
    ) -> None:
        """A SubscribeEventgroup entry from the tester sender must have Type byte 0x06.

        This test does not check the DUT. It checks that the SD entry encoder of the tester
        (`build_subscribe_entry` and `build_sd`) is correct. It sends nothing to the DUT.
        Other tests check that the DUT accepts a Subscribe with Type=0x06 and then sends notifications.
        """
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        raw = encoded_subscribe_entry(tc8_tester_ip, ttl=3)

        type_byte = raw[0]
        assert type_byte == 0x06, f"SubscribeEventgroup entry Type byte must be 0x06; got 0x{type_byte:02x}"


class TestSdStopSubscribeFormat:
    """Byte format of the StopSubscribeEventgroup entry.

    The Open Alliance test specification deleted this test case. The check is still useful:
    a StopSubscribeEventgroup entry is a SubscribeEventgroup entry (Type 0x06) with TTL=0x000000.
    The test makes the entry in memory and examines it. It does not use the DUT.
    """

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_stop_sub_fmt"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_sd_message_12_stop_subscribe_entry_format(self) -> None:
        """The StopSubscribeEventgroup entry has Type=0x06 and TTL=0."""
        entry = encoded_subscribe_entry("127.0.0.1", ttl=0)

        assert len(entry) == 16, f"StopSubscribeEventgroup entry must be 16 bytes; got {len(entry)}"

        type_byte = entry[0]
        assert type_byte == 0x06, (
            f"StopSubscribeEventgroup Type byte must be 0x06 (same as SubscribeEventgroup); got 0x{type_byte:02x}"
        )

        ttl_bytes = entry[9:12]
        assert ttl_bytes == b"\x00\x00\x00", (
            f"StopSubscribeEventgroup TTL bytes[9:12] must be b'\\x00\\x00\\x00'; got {ttl_bytes!r}"
        )


#: Number of Subscribes the Session ID wrap test sends before it reads the NAcks. The socket buffers can hold them.
_WRAP_BURST: int = 64

#: Number of NAcks the Session ID wrap test checks after the Session ID of the DUT goes back to 1.
_WRAP_TAIL: int = 16


class TestSdSessionIdWrap:
    """The unicast SD Session ID of the DUT for one peer goes from 0xFFFF back to 1.

    The Open Alliance case counts OfferService replies to FindService. The DUT does not send these replies.
    NAcks to Subscribes for an unknown service use the same counter for each peer, and the DUT sends them immediately.
    The class has its own class-scoped DUT, so the tester got no unicast message before the first NAck.
    """

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_format_fields"],
        partially_verifies=[
            "AS__PRS_SOMEIPSD_00157",
            "AS__PRS_SOMEIPSD_00158",
            "AS__PRS_SOMEIPSD_00159",
            "AS__PRS_SOMEIPSD_00160",
            "AS__PRS_SOMEIPSD_00255",
            "SOMEIP_ETS_152",
        ],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_ets_152_session_id_wraps_to_one(
        self, someipd_dut: TargetStack, tc8_dut_ip: str, tc8_tester_ip: str
    ) -> None:
        """0xFFFF + 16 NAcks have the Session IDs 1..0xFFFF and then 1..16.

        Variant: the case counts OfferService replies to FindService. This test counts NAcks, which use the same counter.
        The Reboot Flag is 1 on each NAck before the wrap and 0 on each NAck after the wrap.
        """
        assert someipd_dut.poll() is None, "someipd DUT is not running"
        total = 0xFFFF + _WRAP_TAIL
        replies = []
        sock = open_sender_socket(tc8_tester_ip)
        try:
            while len(replies) < total:
                burst = min(_WRAP_BURST, total - len(replies))
                for _ in range(burst):
                    send_subscribe_eventgroup(
                        sock,
                        (tc8_dut_ip, SD_PORT),
                        UNKNOWN_SERVICE_ID,
                        INSTANCE_ID,
                        EVENTGROUP_ID,
                        MAJOR_VERSION,
                        tc8_tester_ip,
                        PROBE_SUBSCRIBER_PORT,
                    )
                try:
                    replies.extend(collect_sd_messages_from_socket(sock, count=burst, timeout_secs=2.0))
                except TimeoutError as exc:
                    pytest.fail(
                        f"NAck stream stalled after {len(replies)} of {total} "
                        f"(next expected Session ID {len(replies) % 0xFFFF + 1:#06x}): {exc}"
                    )
        finally:
            sock.close()

        for i, (header, sd) in enumerate(replies):
            want_id = i % 0xFFFF + 1
            assert any(e.sd_type == SdEntryType.SubscribeAck and e.ttl == 0 for e in sd.entries), (
                f"reply {i} (Session ID {header.session_id:#06x}) is not a NAck"
            )
            assert header.session_id == want_id, (
                f"NAck {i} has Session ID {header.session_id:#06x}, expected {want_id:#06x}"
            )
            assert sd.flag_reboot == (i < 0xFFFF), (
                f"NAck {i} (Session ID {want_id:#06x}) has Reboot Flag {sd.flag_reboot}"
            )
