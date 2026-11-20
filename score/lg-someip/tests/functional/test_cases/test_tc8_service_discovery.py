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
"""TC8 Service Discovery tests for the someipd DUT.

The tests check the OfferService format and cyclic timing, the FindService response,
the SubscribeEventgroup Ack, NAck and StopSubscribe behavior, the SD options, TTL expiry,
and the detection of a Tester reboot.
"""

import ipaddress
import socket
import time

import pytest
from attribute_plugin import add_test_properties
from scapy.contrib.automotive.someip import SDOption_IP4_Multicast
from someip_itf.codec import (
    SD_SERVICE_ID,
    L4Proto,
    MalformedSomeip,
    MessageType,
    ReturnCode,
    SdEntry,
    SdEntryType,
    SomeipResponse,
    parse_someip,
)
from someip_itf.tc8.asserts import (
    assert_offer_has_ipv4_endpoint_option,
    assert_sd_offer_entry,
)
from someip_itf.tc8.constants import (
    CYCLIC_OFFER_DELAY_MS,
    DUT_UNRELIABLE_PORT,
    EVENTGROUP_ID,
    INSTANCE_ID,
    MAJOR_VERSION,
    MINOR_VERSION,
    MULTICAST_EVENTGROUP_ID,
    SD_MULTICAST_ADDR,
    SD_PORT,
    SERVICE_ID,
)
from someip_itf.tc8.dut_lifecycle import TargetStack
from someip_itf.tc8.probes import wait_for_main_phase_anchor_offer
from someip_itf.tc8.sd import capture_sd_offers, open_multicast_socket, parse_sd_message
from someip_itf.tc8.sd_sender import (
    build_subscribe_entry,
    capture_some_ip_messages,
    capture_unicast_sd_entries,
    open_sender_socket,
    send_find_service,
    send_sd_entries,
    send_subscribe_eventgroup,
    send_subscribe_eventgroup_reserved_set,
)
from someip_itf.tc8.timing import capture_sd_offers_with_timestamps

_LGSOMEIP_8_XFAIL_REASON = (
    "[lg-someip #8] the daemon answers every FindService with a unicast offer; the multicast answer of "
    "PRS_SOMEIPSD_00423 is not implemented"
)
_LGSOMEIP_9_XFAIL_REASON = (
    "[lg-someip #9] reboot detection compares the peer's SD address with the subscriber endpoint including the "
    "port, so the old subscription keeps its route"
)

# ---------------------------------------------------------------------------
# Module-level configuration
# ---------------------------------------------------------------------------

#: The static-field eventgroup that the deployment declares, and IDs that it does not declare.
_STATIC_FIELD_EVENTGROUP_ID: int = 0x4480
_UNKNOWN_EVENTGROUP_ID: int = 0xBEEF
_UNKNOWN_SERVICE_ID: int = 0xBEEF

#: SD configuration values that the deployment declares.
_REQUEST_RESPONSE_DELAY_MS: float = 500.0

#: Time to wait for a unicast SD reply to a FindService. It is request_response_delay plus a margin
#: for capture and scheduling jitter, so the test does not miss a reply that is on time.
_UNICAST_REPLY_WINDOW_SECS: float = (_REQUEST_RESPONSE_DELAY_MS + 200.0) / 1000.0

#: Unknown IDs used in negative tests.
_UNKNOWN_INSTANCE_ID: int = 0xBEEF

#: Known someipd limitation: someipd finds the offered service for a FindService, but sends no reply.
_NO_UNICAST_OFFER_REPLY_XFAIL_REASON = (
    "Known someipd limitation: someipd finds the offered service for a FindService, but sends no reply."
)

#: Same limitation. Also, the instance_id and major_version wildcard check reads the wildcard value
#: from the wrong operand.
_NO_UNICAST_OFFER_REPLY_WILDCARD_XFAIL_REASON = (
    _NO_UNICAST_OFFER_REPLY_XFAIL_REASON + " Also, the instance_id and major_version wildcard match "
    "uses the wrong operand."
)


# ---------------------------------------------------------------------------
# Offer format and cyclic timing
# ---------------------------------------------------------------------------


class TestSDOfferFormat:
    """Offer presence, offer fields, and cyclic offer timing."""

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_offer_format"],
        partially_verifies=["AS__PRS_SOMEIPSD_00500"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_tc8_sd_001_multicast_offer_on_startup(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
    ) -> None:
        """someipd sends at least one SD OfferService on multicast at startup."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        offers = capture_sd_offers(tc8_host_ip, min_count=1, timeout_secs=5.0)

        assert len(offers) >= 1, (
            "No SD OfferService entries received within 5 s. "
            "Make sure that someipd runs and that the SD multicast address and port agree with the config."
        )

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_offer_format"],
        partially_verifies=["AS__PRS_SOMEIPSD_00356"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_tc8_sd_002_offer_entry_format(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
    ) -> None:
        """OfferService entry has correct service ID, instance ID, version, and TTL."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        offers = capture_sd_offers(tc8_host_ip, min_count=1, timeout_secs=5.0)

        service_offers = [e for e in offers if e.service_id == SERVICE_ID]
        assert service_offers, (
            f"No OfferService entry found for service 0x{SERVICE_ID:04x} in {len(offers)} captured SD entries."
        )

        assert_sd_offer_entry(
            service_offers[0],
            expected_service_id=SERVICE_ID,
            expected_instance_id=INSTANCE_ID,
            expected_major_version=MAJOR_VERSION,
            expected_minor_version=MINOR_VERSION,
        )

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_cyclic_timing", "AS__PRS_SOMEIPSD_00412"],
        partially_verifies=["AS__PRS_SOMEIPSD_00413"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_tc8_sd_003_cyclic_offer_timing(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
    ) -> None:
        """In the main phase, the offers repeat at cyclic_offer_delay ±20%."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        # Wait for the main phase. The repetition phase ends approximately 1.5 s after the first offer
        # (200+400+800 ms, repetitions_max=3). One cyclic gap (2000 ms) is added for the main-phase offers.
        time.sleep(3.5)

        # Capture sufficient offers for at least 2 gaps between cycles.
        # The DUT can send more than one SD packet in each cycle (service and eventgroup).
        # Thus the test captures more offers and removes duplicates in a 500 ms window.
        timed = capture_sd_offers_with_timestamps(tc8_host_ip, count=5, timeout_secs=15.0)

        service_offers = [(ts, e) for ts, e in timed if e.service_id == SERVICE_ID]
        assert len(service_offers) >= 2, "Not enough OfferService entries for timing analysis"

        # Merge the offers in a 500 ms window into one cycle timestamp.
        cycle_timestamps: list[float] = [service_offers[0][0]]
        for ts, _ in service_offers[1:]:
            if (ts - cycle_timestamps[-1]) * 1000.0 > 500:
                cycle_timestamps.append(ts)

        assert len(cycle_timestamps) >= 3, (
            f"Only {len(cycle_timestamps)} distinct cycles captured (need at least 3 for 2 gap measurements)"
        )

        lo_ms = CYCLIC_OFFER_DELAY_MS * 0.80
        hi_ms = CYCLIC_OFFER_DELAY_MS * 1.20

        for i in range(len(cycle_timestamps) - 1):
            gap_ms = (cycle_timestamps[i + 1] - cycle_timestamps[i]) * 1000.0
            assert lo_ms <= gap_ms <= hi_ms, (
                f"inter-offer gap {gap_ms:.0f} ms not in "
                f"[{lo_ms:.0f}, {hi_ms:.0f}] ms "
                f"(cyclic_offer_delay={CYCLIC_OFFER_DELAY_MS:.0f} ms ±20%)"
            )


# ---------------------------------------------------------------------------
# FindService response
# ---------------------------------------------------------------------------


class TestSDFindResponse:
    """FindService response for known and unknown services."""

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_find_response"],
        partially_verifies=["AS__PRS_SOMEIPSD_00422", "AS__PRS_SOMEIPSD_00500"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_tc8_sd_004_find_known_service_unicast_offer(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        tc8_tester_ip: str,
    ) -> None:
        """FindService for offered service triggers a unicast OfferService."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        # tc8_tester_ip is not tc8_host_ip. The SD protocol requires SD_PORT on both.
        # Send the FindService as unicast to the DUT. On loopback, multicast between
        # different 127.x addresses is not reliable in some environments.
        sock = open_sender_socket(tc8_tester_ip)
        try:

            def _send_find() -> None:
                send_find_service(
                    sock,
                    (tc8_dut_ip, SD_PORT),
                    service_id=SERVICE_ID,
                    instance_id=INSTANCE_ID,
                )

            _send_find()
            # The provider sends a unicast response to the address of the sender.
            # The DUT can delay the response until the next SD cycle (approximately 2 s).
            # Send again at intervals, because a DUT that runs for a long time can batch or drop duplicates.
            entries = capture_unicast_sd_entries(
                sock,
                filter_types=(SdEntryType.OfferService,),
                timeout_secs=5.0,
                resend=_send_find,
            )
            service_offers = [e for e in entries if e.service_id == SERVICE_ID]
            assert service_offers, (
                f"No unicast OfferService received for service 0x{SERVICE_ID:04x} within 5 s of FindService"
            )
        finally:
            sock.close()

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_find_response"],
        partially_verifies=["AS__PRS_SOMEIPSD_00825"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_tc8_sd_005_find_unknown_service_no_response(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        tc8_tester_ip: str,
    ) -> None:
        """FindService for unknown service does not trigger OfferService."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        sock = open_sender_socket(tc8_tester_ip)
        try:
            send_find_service(
                sock,
                (SD_MULTICAST_ADDR, SD_PORT),
                service_id=_UNKNOWN_SERVICE_ID,
            )
            entries = capture_unicast_sd_entries(
                sock,
                filter_types=(SdEntryType.OfferService,),
                timeout_secs=2.0,
            )
            unknown_offers = [e for e in entries if e.service_id == _UNKNOWN_SERVICE_ID]
            assert not unknown_offers, (
                f"Unexpected OfferService received for unknown service 0x{_UNKNOWN_SERVICE_ID:04x}"
            )
        finally:
            sock.close()


# ---------------------------------------------------------------------------
# SubscribeEventgroup lifecycle
# ---------------------------------------------------------------------------


class TestSDSubscribeLifecycle:
    """Subscribe Ack, NAck, and StopSubscribe."""

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_sub_lifecycle", "AS__PRS_SOMEIPSD_00391"],
        partially_verifies=["AS__PRS_SOMEIPSD_00390", "AS__PRS_SOMEIPSD_00466"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_tc8_sd_006_subscribe_valid_eventgroup_ack(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        tc8_tester_ip: str,
    ) -> None:
        """A valid SubscribeEventgroup gets a SubscribeEventgroupAck that repeats the Subscribe fields."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        # The Ack must have the same TTL as the Subscribe. Thus the test sets the TTL.
        _TTL_SECS = 3

        # The DUT sends the Ack to the source address of the Subscribe.
        # The DUT can delay the Ack until the next SD cycle (approximately 2 s).
        sock = open_sender_socket(tc8_tester_ip)
        try:
            sender_port = sock.getsockname()[1]  # = SD_PORT

            def _send_subscribe() -> None:
                send_subscribe_eventgroup(
                    sock,
                    (tc8_dut_ip, SD_PORT),
                    SERVICE_ID,
                    INSTANCE_ID,
                    EVENTGROUP_ID,
                    MAJOR_VERSION,
                    subscriber_ip=tc8_tester_ip,
                    subscriber_port=sender_port,
                    ttl=_TTL_SECS,
                )

            _send_subscribe()
            entries = capture_unicast_sd_entries(
                sock,
                filter_types=(SdEntryType.SubscribeAck,),
                timeout_secs=5.0,
                resend=_send_subscribe,
            )
            acks = [
                e
                for e in entries
                if e.service_id == SERVICE_ID and e.instance_id == INSTANCE_ID and e.eventgroup_id == EVENTGROUP_ID
            ]
            assert acks, (
                f"No SubscribeEventgroupAck received for service 0x{SERVICE_ID:04x}, "
                f"instance 0x{INSTANCE_ID:04x}, eventgroup 0x{EVENTGROUP_ID:04x}"
            )

            # Each field below must agree with the Subscribe that the test sent.
            ack = acks[0]
            assert ack.major_version == MAJOR_VERSION, (
                f"Ack major_version 0x{ack.major_version:02x}, expected 0x{MAJOR_VERSION:02x}"
            )
            assert ack.ttl == _TTL_SECS, f"Ack TTL {ack.ttl}, expected {_TTL_SECS} as in the Subscribe"
            assert ack.eventgroup_counter == 0, f"Ack counter {ack.eventgroup_counter}, expected 0"
            # Bits 31-20 of minver_or_counter are reserved. The Subscribe sends 0 in these bits.
            reserved = (ack.minver_or_counter >> 20) & 0xFFF
            assert reserved == 0, f"Ack reserved bits 0x{reserved:03x}, expected 0"
        finally:
            sock.close()

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_sub_lifecycle", "AS__PRS_SOMEIPSD_00393"],
        partially_verifies=["AS__PRS_SOMEIPSD_00828"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_tc8_sd_007_subscribe_unknown_eventgroup_nack(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        tc8_tester_ip: str,
    ) -> None:
        """A SubscribeEventgroup for an unknown eventgroup gets a NAck (TTL=0)."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        sock = open_sender_socket(tc8_tester_ip)
        try:
            sender_port = sock.getsockname()[1]  # = SD_PORT

            def _send_subscribe_unknown() -> None:
                send_subscribe_eventgroup(
                    sock,
                    (tc8_dut_ip, SD_PORT),
                    SERVICE_ID,
                    INSTANCE_ID,
                    _UNKNOWN_EVENTGROUP_ID,
                    MAJOR_VERSION,
                    subscriber_ip=tc8_tester_ip,
                    subscriber_port=sender_port,
                )

            _send_subscribe_unknown()
            entries = capture_unicast_sd_entries(
                sock,
                filter_types=(SdEntryType.SubscribeAck,),
                timeout_secs=5.0,
                resend=_send_subscribe_unknown,
            )
            nacks = [e for e in entries if e.eventgroup_id == _UNKNOWN_EVENTGROUP_ID and e.ttl == 0]
            assert nacks, (
                f"No SubscribeEventgroupNack received for unknown eventgroup "
                f"0x{_UNKNOWN_EVENTGROUP_ID:04x} (expected SubscribeAck with TTL=0)"
            )
        finally:
            sock.close()

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_sub_lifecycle"],
        partially_verifies=["AS__PRS_SOMEIPSD_00453", "AS__PRS_SOMEIPSD_00466"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_tc8_sd_008_stop_subscribe_ceases_notifications(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        tc8_tester_ip: str,
    ) -> None:
        """A StopSubscribeEventgroup (TTL=0) stops the event notifications."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        # sd_sock: SD sender at tc8_tester_ip:SD_PORT.
        # notif_sock: gets event notifications at tc8_tester_ip:<ephemeral>.
        sd_sock = open_sender_socket(tc8_tester_ip)
        notif_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        notif_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        notif_sock.bind((tc8_tester_ip, 0))
        notif_port: int = notif_sock.getsockname()[1]

        try:
            # The TTL must be longer than the test, so that only the StopSubscribe can end the subscription.
            _TTL_SECS = 30

            def _send_sub_008() -> None:
                send_subscribe_eventgroup(
                    sd_sock,
                    (tc8_dut_ip, SD_PORT),
                    SERVICE_ID,
                    INSTANCE_ID,
                    EVENTGROUP_ID,
                    MAJOR_VERSION,
                    subscriber_ip=tc8_tester_ip,
                    subscriber_port=notif_port,
                    ttl=_TTL_SECS,
                )

            _send_sub_008()
            acks = capture_unicast_sd_entries(
                sd_sock,
                filter_types=(SdEntryType.SubscribeAck,),
                timeout_secs=5.0,
                resend=_send_sub_008,
                max_results=1,
            )
            assert any(e.eventgroup_id == EVENTGROUP_ID and e.ttl > 0 for e in acks), (
                "Precondition failed: no SubscribeEventgroupAck received"
            )

            # Expect at least one notification. The DUT sends a notification every 2 s.
            notifs = capture_some_ip_messages(notif_sock, SERVICE_ID, timeout_secs=4.0)
            assert notifs, "No SOME/IP notifications received after subscribe"

            # Stop the subscription (TTL=0).
            send_subscribe_eventgroup(
                sd_sock,
                (tc8_dut_ip, SD_PORT),
                SERVICE_ID,
                INSTANCE_ID,
                EVENTGROUP_ID,
                MAJOR_VERSION,
                subscriber_ip=tc8_tester_ip,
                subscriber_port=notif_port,
                ttl=0,
            )

            # Discard a notification that the DUT can send before it gets the StopSubscribe.
            capture_some_ip_messages(notif_sock, SERVICE_ID, timeout_secs=0.5)

            # Two notification periods. This is much shorter than the 30 s TTL.
            post = capture_some_ip_messages(notif_sock, SERVICE_ID, timeout_secs=4.0)
            assert not post, f"{len(post)} notification(s) received after StopSubscribeEventgroup"
        finally:
            sd_sock.close()
            notif_sock.close()


# ---------------------------------------------------------------------------
# SD option format (IPv4 endpoint option)
# ---------------------------------------------------------------------------


class TestSDOptionFormat:
    """The OfferService entry has a valid IPv4EndpointOption."""

    @add_test_properties(
        fully_verifies=[
            "comp_req__tc8_conformance__sd_endpoint_option",
            "AS__PRS_SOMEIPSD_00306",
            "AS__PRS_SOMEIPSD_00307",
            "AS__PRS_SOMEIPSD_00357",
        ],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_tc8_sd_011_offer_ipv4_endpoint_option(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
    ) -> None:
        """The SD OfferService entry has an IPv4EndpointOption with the correct address, port and protocol."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        # Capture raw SD packets to examine the options of the entries.
        sock = open_multicast_socket(tc8_host_ip)
        try:
            deadline = time.monotonic() + 5.0
            found_entry = None
            while time.monotonic() < deadline and found_entry is None:
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    break
                sock.settimeout(min(remaining, 1.0))
                try:
                    data, _ = sock.recvfrom(65535)
                except socket.timeout:
                    continue
                parsed = parse_sd_message(data)
                if parsed is None:
                    continue
                for entry in parsed[1].entries:
                    if entry.sd_type == SdEntryType.OfferService and entry.service_id == SERVICE_ID:
                        found_entry = entry
                        break
        finally:
            sock.close()

        assert found_entry is not None, f"No OfferService entry found for service 0x{SERVICE_ID:04x} within 5 s"
        assert_offer_has_ipv4_endpoint_option(
            found_entry,
            expected_ip=tc8_dut_ip,
            expected_port=DUT_UNRELIABLE_PORT,
        )


# ---------------------------------------------------------------------------
# Multicast eventgroup option in SUBSCRIBE_ACK
# ---------------------------------------------------------------------------


class TestSDMulticastEventgroup:
    """The SUBSCRIBE_ACK for a multicast eventgroup has a multicast IP option."""

    @pytest.mark.network
    @add_test_properties(
        fully_verifies=[
            "comp_req__tc8_conformance__sd_mcast_eg",
            "AS__PRS_SOMEIPSD_00324",
            "AS__PRS_SOMEIPSD_00325",
            "AS__PRS_SOMEIPSD_00326",
            "AS__PRS_SOMEIPSD_00392",
        ],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_tc8_sd_013_subscribe_ack_has_multicast_option(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        tc8_tester_ip: str,
    ) -> None:
        """A Subscribe to multicast eventgroup 0x4465 gets a multicast endpoint option."""

        # On loopback, vsomeip 3.6.1 does not put an IPv4MulticastOption in the
        # SUBSCRIBE_ACK. The test requires an interface that is not loopback.
        if ipaddress.ip_address(tc8_host_ip).is_loopback:
            pytest.skip(
                "Multicast endpoint option in SUBSCRIBE_ACK requires an interface that is not loopback. "
                "Set TC8_HOST_IP to an address that is not loopback (for example TC8_HOST_IP=192.168.x.y)."
            )

        assert someipd_dut.poll() is None, "someipd DUT is not running"

        sock = open_sender_socket(tc8_tester_ip)
        try:
            sender_port = sock.getsockname()[1]

            def _send_subscribe_multicast() -> None:
                send_subscribe_eventgroup(
                    sock,
                    (tc8_dut_ip, SD_PORT),
                    SERVICE_ID,
                    INSTANCE_ID,
                    MULTICAST_EVENTGROUP_ID,
                    MAJOR_VERSION,
                    subscriber_ip=tc8_tester_ip,
                    subscriber_port=sender_port,
                )

            _send_subscribe_multicast()
            entries = capture_unicast_sd_entries(
                sock,
                filter_types=(SdEntryType.SubscribeAck,),
                timeout_secs=5.0,
                resend=_send_subscribe_multicast,
            )
            acks = [e for e in entries if e.eventgroup_id == MULTICAST_EVENTGROUP_ID and e.ttl > 0]
            # pytest.fail raises Failed, not AssertionError. Thus a missing ACK is not covered by the xfail for the option.
            if not acks:
                pytest.fail(
                    f"No SubscribeEventgroupAck received for multicast eventgroup 0x{MULTICAST_EVENTGROUP_ID:04x}"
                )
            ack = acks[0]
            # The ACK for a multicast eventgroup must have an IPv4MulticastOption (SD option type 0x14).
            # This is not the IPv4EndpointOption (0x04) for unicast endpoints.
            # A check for SDOption_IP4_EndPoint would ignore all multicast options.
            options = list(ack.options_1) + list(ack.options_2)
            multicast_opts = [
                o
                for o in options
                if isinstance(o, SDOption_IP4_Multicast) and ipaddress.ip_address(o.addr).is_multicast
            ]
            assert multicast_opts, (
                f"SUBSCRIBE_ACK for eventgroup 0x{MULTICAST_EVENTGROUP_ID:04x} "
                f"does not have an IPv4MulticastOption (SD option type 0x14). "
                f"Options found: {options}"
            )
            opt = multicast_opts[0]
            assert opt.len == 0x0009, f"multicast option length 0x{opt.len:04x}, expected 0x0009"
            assert opt.res_hdr == 0x00, f"discardable flag/reserved bits 0x{opt.res_hdr:02x}, expected 0x00"
            assert opt.res_tail == 0x00, f"reserved byte 0x{opt.res_tail:02x}, expected 0x00"
            assert opt.l4_proto == L4Proto.UDP, f"L4-Proto 0x{opt.l4_proto:02x}, expected UDP (0x11)"
            assert opt.port != 0, "multicast option port is 0"
        finally:
            sock.close()


# ---------------------------------------------------------------------------
# TTL expiry
# ---------------------------------------------------------------------------


class TestSDTTLExpiry:
    """The DUT removes a subscription with a finite TTL when the TTL expires."""

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_sub_lifecycle"],
        partially_verifies=["AS__PRS_SOMEIPSD_00466"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_tc8_sd_014_ttl_expiry_ceases_notifications(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        tc8_tester_ip: str,
    ) -> None:
        """The DUT sends no notifications after the subscription TTL expires."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        sd_sock = open_sender_socket(tc8_tester_ip)
        notif_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        notif_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        notif_sock.bind((tc8_tester_ip, 0))
        notif_port: int = notif_sock.getsockname()[1]

        try:
            # Subscribe with a short TTL (3 s).
            _TTL_SECS = 3

            def _send_sub_ttl() -> None:
                send_subscribe_eventgroup(
                    sd_sock,
                    (tc8_dut_ip, SD_PORT),
                    SERVICE_ID,
                    INSTANCE_ID,
                    EVENTGROUP_ID,
                    MAJOR_VERSION,
                    subscriber_ip=tc8_tester_ip,
                    subscriber_port=notif_port,
                    ttl=_TTL_SECS,
                )

            _send_sub_ttl()
            acks = capture_unicast_sd_entries(
                sd_sock,
                filter_types=(SdEntryType.SubscribeAck,),
                timeout_secs=5.0,
                resend=_send_sub_ttl,
            )
            assert any(e.eventgroup_id == EVENTGROUP_ID and e.ttl > 0 for e in acks), (
                "Precondition failed: no SubscribeEventgroupAck received"
            )

            # Make sure that at least one notification arrives before the TTL expires.
            pre_expiry = capture_some_ip_messages(notif_sock, SERVICE_ID, timeout_secs=4.0)
            assert pre_expiry, "No notifications received before TTL expiry"

            # Wait until the TTL expires (TTL + 2 s margin).
            time.sleep(_TTL_SECS + 2)

            # Make sure that no more notifications arrive in a 3 s window.
            post_expiry = capture_some_ip_messages(notif_sock, SERVICE_ID, timeout_secs=3.0)
            assert not post_expiry, (
                f"{len(post_expiry)} notification(s) received after TTL expiry ({_TTL_SECS} s + 2 s margin)"
            )
        finally:
            sd_sock.close()
            notif_sock.close()


# ---------------------------------------------------------------------------
# FindService instance and version matching
# ---------------------------------------------------------------------------


class TestSDVersionMatching:
    """FindService with wildcard and specific instance and version values."""

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_find_response"],
        partially_verifies=[
            "AS__PRS_SOMEIPSD_00422",
            "AS__PRS_SOMEIPSD_00825",
            "SOMEIPSRV_SD_MESSAGE_01",
        ],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_sd_message_01_instance_wildcard(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        tc8_tester_ip: str,
    ) -> None:
        """A FindService with instance_id=0xFFFF gets an offer for the specific instance."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        sock = open_sender_socket(tc8_tester_ip)
        try:

            def _send() -> None:
                send_find_service(
                    sock,
                    (tc8_dut_ip, SD_PORT),
                    service_id=SERVICE_ID,
                    instance_id=0xFFFF,
                )

            _send()
            entries = capture_unicast_sd_entries(
                sock,
                filter_types=(SdEntryType.OfferService,),
                timeout_secs=5.0,
                resend=_send,
            )
            matching = [e for e in entries if e.service_id == SERVICE_ID]
            assert matching, "No OfferService received for instance_id=0xFFFF (wildcard) FindService"
            assert matching[0].instance_id == INSTANCE_ID, (
                f"OfferService instance_id 0x{matching[0].instance_id:04x} != 0x{INSTANCE_ID:04x}"
            )
        finally:
            sock.close()

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_find_response"],
        partially_verifies=[
            "AS__PRS_SOMEIPSD_00422",
            "AS__PRS_SOMEIPSD_00825",
            "SOMEIPSRV_SD_MESSAGE_02",
        ],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_sd_message_02_instance_specific(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        tc8_tester_ip: str,
    ) -> None:
        """A FindService with a specific instance_id gets an offer for that instance."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        sock = open_sender_socket(tc8_tester_ip)
        try:

            def _send() -> None:
                send_find_service(
                    sock,
                    (tc8_dut_ip, SD_PORT),
                    service_id=SERVICE_ID,
                    instance_id=INSTANCE_ID,
                )

            _send()
            entries = capture_unicast_sd_entries(
                sock,
                filter_types=(SdEntryType.OfferService,),
                timeout_secs=5.0,
                resend=_send,
            )
            matching = [e for e in entries if e.service_id == SERVICE_ID and e.instance_id == INSTANCE_ID]
            assert matching, f"No OfferService received for specific instance_id=0x{INSTANCE_ID:04x}"
        finally:
            sock.close()

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_find_response"],
        partially_verifies=[
            "AS__PRS_SOMEIPSD_00422",
            "AS__PRS_SOMEIPSD_00825",
            "SOMEIPSRV_SD_MESSAGE_03",
        ],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_sd_message_03_major_version_wildcard(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        tc8_tester_ip: str,
    ) -> None:
        """A FindService with major_version=0xFF (any) gets an offer for the service."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        sock = open_sender_socket(tc8_tester_ip)
        try:

            def _send() -> None:
                send_find_service(
                    sock,
                    (tc8_dut_ip, SD_PORT),
                    service_id=SERVICE_ID,
                    instance_id=INSTANCE_ID,
                    major_version=0xFF,
                )

            _send()
            entries = capture_unicast_sd_entries(
                sock,
                filter_types=(SdEntryType.OfferService,),
                timeout_secs=5.0,
                resend=_send,
            )
            matching = [e for e in entries if e.service_id == SERVICE_ID]
            assert matching, "No OfferService received for major_version=0xFF (wildcard) FindService"
        finally:
            sock.close()

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_find_response"],
        partially_verifies=[
            "AS__PRS_SOMEIPSD_00422",
            "AS__PRS_SOMEIPSD_00825",
            "SOMEIPSRV_SD_MESSAGE_04",
        ],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_sd_message_04_major_version_specific(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        tc8_tester_ip: str,
    ) -> None:
        """A FindService with a specific major_version gets an offer for the service."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        sock = open_sender_socket(tc8_tester_ip)
        try:

            def _send() -> None:
                send_find_service(
                    sock,
                    (tc8_dut_ip, SD_PORT),
                    service_id=SERVICE_ID,
                    instance_id=INSTANCE_ID,
                    major_version=MAJOR_VERSION,
                )

            _send()
            entries = capture_unicast_sd_entries(
                sock,
                filter_types=(SdEntryType.OfferService,),
                timeout_secs=5.0,
                resend=_send,
            )
            matching = [e for e in entries if e.service_id == SERVICE_ID and e.major_version == MAJOR_VERSION]
            assert matching, f"No OfferService received for specific major_version=0x{MAJOR_VERSION:02x}"
        finally:
            sock.close()

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_find_response"],
        partially_verifies=[
            "AS__PRS_SOMEIPSD_00422",
            "AS__PRS_SOMEIPSD_00825",
            "SOMEIPSRV_SD_MESSAGE_05",
        ],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_sd_message_05_minor_version_wildcard(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        tc8_tester_ip: str,
    ) -> None:
        """A FindService with minor_version=0xFFFFFFFF (any) gets an offer for the service."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        sock = open_sender_socket(tc8_tester_ip)
        try:

            def _send() -> None:
                send_find_service(
                    sock,
                    (tc8_dut_ip, SD_PORT),
                    service_id=SERVICE_ID,
                    instance_id=INSTANCE_ID,
                    major_version=MAJOR_VERSION,
                    minor_version=0xFFFFFFFF,
                )

            _send()
            entries = capture_unicast_sd_entries(
                sock,
                filter_types=(SdEntryType.OfferService,),
                timeout_secs=5.0,
                resend=_send,
            )
            matching = [e for e in entries if e.service_id == SERVICE_ID]
            assert matching, "No OfferService received for minor_version=0xFFFFFFFF (wildcard) FindService"
        finally:
            sock.close()

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_find_response"],
        partially_verifies=[
            "AS__PRS_SOMEIPSD_00422",
            "AS__PRS_SOMEIPSD_00825",
            "SOMEIPSRV_SD_MESSAGE_06",
        ],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_sd_message_06_minor_version_specific(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        tc8_tester_ip: str,
    ) -> None:
        """A FindService with the specific minor_version=0x00000000 gets an offer for the service."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        sock = open_sender_socket(tc8_tester_ip)
        try:

            def _send() -> None:
                send_find_service(
                    sock,
                    (tc8_dut_ip, SD_PORT),
                    service_id=SERVICE_ID,
                    instance_id=INSTANCE_ID,
                    major_version=MAJOR_VERSION,
                    minor_version=MINOR_VERSION,
                )

            _send()
            entries = capture_unicast_sd_entries(
                sock,
                filter_types=(SdEntryType.OfferService,),
                timeout_secs=5.0,
                resend=_send,
            )
            matching = [e for e in entries if e.service_id == SERVICE_ID]
            assert matching, f"No OfferService received for specific minor_version=0x{MINOR_VERSION:08x}"
        finally:
            sock.close()


# ---------------------------------------------------------------------------
# SubscribeEventgroup NAck
# ---------------------------------------------------------------------------


class TestSDSubscribeNAck:
    """SubscribeEventgroup NAck cases.

    A SubscribeEventgroup NAck is a SubscribeAck SD entry (type 0x07) with TTL=0.
    """

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_sub_lifecycle", "AS__PRS_SOMEIPSD_00393"],
        partially_verifies=[
            "AS__PRS_SOMEIPSD_00566",
            "AS__PRS_SOMEIPSD_00828",
            "SOMEIPSRV_SD_MESSAGE_18",
        ],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_sd_message_18_wrong_major_version(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        tc8_tester_ip: str,
    ) -> None:
        """A Subscribe with a wrong major_version gets a NAck (TTL=0)."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        sock = open_sender_socket(tc8_tester_ip)
        try:
            sender_port = sock.getsockname()[1]

            def _send() -> None:
                send_subscribe_eventgroup(
                    sock,
                    (tc8_dut_ip, SD_PORT),
                    SERVICE_ID,
                    INSTANCE_ID,
                    EVENTGROUP_ID,
                    major_version=0xFF,  # The DUT expects MAJOR_VERSION
                    subscriber_ip=tc8_tester_ip,
                    subscriber_port=sender_port,
                )

            _send()
            entries = capture_unicast_sd_entries(
                sock,
                filter_types=(SdEntryType.SubscribeAck,),
                timeout_secs=3.0,
                resend=_send,
            )
            nacks = [e for e in entries if e.eventgroup_id == EVENTGROUP_ID and e.ttl == 0]
            assert nacks, "No SubscribeEventgroupNAck (TTL=0) received for wrong major_version=0xFF"
        finally:
            sock.close()

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_sub_lifecycle", "AS__PRS_SOMEIPSD_00393"],
        partially_verifies=[
            "AS__PRS_SOMEIPSD_00566",
            "AS__PRS_SOMEIPSD_00828",
            "SOMEIPSRV_SD_MESSAGE_15",
        ],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_sd_message_15_wrong_service_id(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        tc8_tester_ip: str,
    ) -> None:
        """A Subscribe to an unknown service_id gets a NAck (TTL=0).

        The NAck is a SubscribeAck entry with TTL=0. The test checks five fields of the NAck:
        Instance ID, Eventgroup ID, TTL=0, Reserved=0 and Major Version.
        """
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        sock = open_sender_socket(tc8_tester_ip)
        try:
            sender_port = sock.getsockname()[1]

            def _send() -> None:
                send_subscribe_eventgroup(
                    sock,
                    (tc8_dut_ip, SD_PORT),
                    service_id=_UNKNOWN_SERVICE_ID,
                    instance_id=INSTANCE_ID,
                    eventgroup_id=EVENTGROUP_ID,
                    major_version=MAJOR_VERSION,
                    subscriber_ip=tc8_tester_ip,
                    subscriber_port=sender_port,
                )

            _send()
            entries = capture_unicast_sd_entries(
                sock,
                filter_types=(SdEntryType.SubscribeAck,),
                timeout_secs=3.0,
                resend=_send,
            )
            nacks = [e for e in entries if e.eventgroup_id == EVENTGROUP_ID and e.ttl == 0]
            assert nacks, (
                f"No SubscribeEventgroupNAck (TTL=0) received for unknown service_id=0x{_UNKNOWN_SERVICE_ID:04x}"
            )
            nack = nacks[0]
            assert nack.instance_id == INSTANCE_ID, (
                f"NAck instance_id must echo 0x{INSTANCE_ID:04x}; got 0x{nack.instance_id:04x}"
            )
            assert nack.major_version == MAJOR_VERSION, (
                f"NAck major_version must echo 0x{MAJOR_VERSION:02x}; got 0x{nack.major_version:02x}"
            )
            reserved = (nack.minver_or_counter >> 16) & 0xFFFF
            assert reserved == 0, f"NAck Reserved field must be 0; got 0x{reserved:04x}"
        finally:
            sock.close()

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_sub_lifecycle", "AS__PRS_SOMEIPSD_00393"],
        partially_verifies=[
            "AS__PRS_SOMEIPSD_00566",
            "AS__PRS_SOMEIPSD_00828",
            "SOMEIPSRV_SD_MESSAGE_17",
        ],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_sd_message_17_wrong_instance_id(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        tc8_tester_ip: str,
    ) -> None:
        """A Subscribe to a wrong instance_id gets a NAck (TTL=0).

        The Service Instance ID field of the NAck repeats the wrong instance_id that the test sent.
        """
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        sock = open_sender_socket(tc8_tester_ip)
        try:
            sender_port = sock.getsockname()[1]

            def _send() -> None:
                send_subscribe_eventgroup(
                    sock,
                    (tc8_dut_ip, SD_PORT),
                    SERVICE_ID,
                    instance_id=_UNKNOWN_INSTANCE_ID,
                    eventgroup_id=EVENTGROUP_ID,
                    major_version=MAJOR_VERSION,
                    subscriber_ip=tc8_tester_ip,
                    subscriber_port=sender_port,
                )

            _send()
            entries = capture_unicast_sd_entries(
                sock,
                filter_types=(SdEntryType.SubscribeAck,),
                timeout_secs=3.0,
                resend=_send,
            )
            nacks = [e for e in entries if e.eventgroup_id == EVENTGROUP_ID and e.ttl == 0]
            assert nacks, (
                f"No SubscribeEventgroupNAck (TTL=0) received for wrong instance_id=0x{_UNKNOWN_INSTANCE_ID:04x}"
            )
            assert nacks[0].instance_id == _UNKNOWN_INSTANCE_ID, (
                f"NAck instance_id must echo the wrong instance_id "
                f"0x{_UNKNOWN_INSTANCE_ID:04x} that was sent; got 0x{nacks[0].instance_id:04x}"
            )
        finally:
            sock.close()

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_sub_lifecycle", "AS__PRS_SOMEIPSD_00393"],
        partially_verifies=[
            "AS__PRS_SOMEIPSD_00566",
            "AS__PRS_SOMEIPSD_00828",
            "SOMEIPSRV_SD_MESSAGE_19",
        ],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_sd_message_19_unknown_eventgroup_id(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        tc8_tester_ip: str,
    ) -> None:
        """A Subscribe to an unknown eventgroup_id gets a NAck (TTL=0)."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        sock = open_sender_socket(tc8_tester_ip)
        try:
            sender_port = sock.getsockname()[1]

            def _send() -> None:
                send_subscribe_eventgroup(
                    sock,
                    (tc8_dut_ip, SD_PORT),
                    SERVICE_ID,
                    INSTANCE_ID,
                    eventgroup_id=_UNKNOWN_EVENTGROUP_ID,
                    major_version=MAJOR_VERSION,
                    subscriber_ip=tc8_tester_ip,
                    subscriber_port=sender_port,
                )

            _send()
            entries = capture_unicast_sd_entries(
                sock,
                filter_types=(SdEntryType.SubscribeAck,),
                timeout_secs=5.0,
                resend=_send,
            )
            nacks = [e for e in entries if e.eventgroup_id == _UNKNOWN_EVENTGROUP_ID and e.ttl == 0]
            assert nacks, (
                f"No SubscribeEventgroupNAck (TTL=0) received for unknown eventgroup_id=0x{_UNKNOWN_EVENTGROUP_ID:04x}"
            )
        finally:
            sock.close()

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_sub_lifecycle"],
        partially_verifies=["AS__PRS_SOMEIPSD_00391"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    @pytest.mark.xfail(
        strict=True,
        reason=(
            "Known someipd limitation: the DUT sends a positive SubscribeEventgroupAck "
            "(TTL > 0) when the Subscribe entry has reserved bits set. The correct response "
            "is a NAck (TTL = 0) or no response."
        ),
    )
    def test_sd_subscribe_reserved_field_set(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        tc8_tester_ip: str,
    ) -> None:
        """A Subscribe with the reserved field set gets a NAck or no response, not a positive Ack."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        sock = open_sender_socket(tc8_tester_ip)
        try:
            sender_port = sock.getsockname()[1]

            def _send() -> None:
                send_subscribe_eventgroup_reserved_set(
                    sock,
                    (tc8_dut_ip, SD_PORT),
                    SERVICE_ID,
                    INSTANCE_ID,
                    EVENTGROUP_ID,
                    MAJOR_VERSION,
                    subscriber_ip=tc8_tester_ip,
                    subscriber_port=sender_port,
                    reserved_value=0x0F,
                )

            _send()
            entries = capture_unicast_sd_entries(
                sock,
                filter_types=(SdEntryType.SubscribeAck,),
                timeout_secs=3.0,
                resend=_send,
            )
            # Accept a NAck (TTL=0) or no response (the DUT ignores the entry).
            # A positive Ack (TTL>0) shows that the DUT accepted the malformed entry.
            acks = [e for e in entries if e.eventgroup_id == EVENTGROUP_ID and e.ttl > 0]
            assert not acks, (
                "DUT sent a positive SubscribeAck (TTL>0) for a SubscribeEventgroup entry "
                "with reserved bits set. Expected a NAck or no response."
            )
        finally:
            sock.close()


# ---------------------------------------------------------------------------
# FindService response timing
# ---------------------------------------------------------------------------


class TestSDFindServiceTiming:
    """Timing of the response to a FindService."""

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_cyclic_timing"],
        partially_verifies=["AS__PRS_SOMEIPSD_00423", "SOMEIPSRV_SD_BEHAVIOR_03"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    @pytest.mark.xfail(strict=True, reason=_LGSOMEIP_8_XFAIL_REASON)
    def test_sd_behavior_03_unicast_findservice_timing(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        tc8_tester_ip: str,
    ) -> None:
        """A FindService after an old offer gets a multicast reply, not a unicast reply.

        In the Main Phase, a FindService with the Unicast Flag set to 1 can arrive at least
        half of ``cyclic_offer_delay`` after the last OfferService. Here this is 1000 ms,
        because cyclic_offer_delay is 2000 ms. The DUT must answer with the next
        *multicast* OfferService, not with a unicast reply. The test waits for the Main Phase
        and for an offer. Then it waits for half the delay and sends the FindService.
        It makes sure that the response arrives on the SD multicast group, not on the sender socket.

        A multicast offer alone does not prove that the DUT answers this FindService,
        because the cyclic timer always sends multicast offers. Thus the important check is
        that no unicast reply arrives. The DUT does not answer a FindService at all.
        A different test in this class shows this.
        """
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        last_offer_ts = wait_for_main_phase_anchor_offer(tc8_host_ip)

        # Wait until at least half of cyclic_offer_delay has passed since that offer.
        half_delay_secs = (CYCLIC_OFFER_DELAY_MS / 2.0) / 1000.0
        remaining_wait = half_delay_secs - (time.monotonic() - last_offer_ts)
        if remaining_wait > 0:
            time.sleep(remaining_wait)

        send_sock = open_sender_socket(tc8_tester_ip)
        try:
            send_find_service(
                send_sock,
                (tc8_dut_ip, SD_PORT),
                service_id=SERVICE_ID,
                instance_id=INSTANCE_ID,
                major_version=MAJOR_VERSION,
                flag_unicast=True,  # Precondition: Unicast Flag = 1
            )

            _max_allowed_secs = (CYCLIC_OFFER_DELAY_MS * 1.5) / 1000.0
            try:
                offers = capture_sd_offers(tc8_host_ip, min_count=1, timeout_secs=_max_allowed_secs)
            except TimeoutError:
                offers = []
            matching = [e for e in offers if e.service_id == SERVICE_ID]

            assert matching, (
                "No OfferService received on the SD multicast "
                f"group within {_max_allowed_secs:.1f} s of a Unicast-Flag=1 FindService "
                f"sent >= half cyclic_offer_delay ({half_delay_secs * 1000:.0f} ms) after "
                "the last offer"
            )

            # A unicast reply arrives on the socket of the FindService sender, not on the multicast
            # group. Make sure that no reply arrives there. Wait the full request_response_delay
            # (plus a margin), so that the test does not miss a reply.
            unicast_reply = capture_unicast_sd_entries(
                send_sock,
                filter_types=(SdEntryType.OfferService,),
                timeout_secs=_UNICAST_REPLY_WINDOW_SECS,
            )
            unicast_matching = [e for e in unicast_reply if e.service_id == SERVICE_ID]
            assert not unicast_matching, (
                "OfferService arrived unicast to the FindService "
                "sender. A multicast response is necessary when the last offer is "
                ">= half cyclic_offer_delay old"
            )
        finally:
            send_sock.close()

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_cyclic_timing"],
        partially_verifies=[
            "AS__PRS_SOMEIPSD_00422",
            "AS__PRS_SOMEIPSD_00423",
            "SOMEIPSRV_SD_BEHAVIOR_03",
        ],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_sd_behavior_03_fresh_offer_findservice_gets_unicast_reply(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        tc8_tester_ip: str,
    ) -> None:
        """A FindService after a new offer gets a unicast reply.

        The multicast rule applies only when the last offer is at least half of cyclic_offer_delay old.
        While the offer is new, a FindService with Unicast Flag 1 must get a unicast OfferService
        in request_response_delay. The test waits for an offer and sends the FindService immediately,
        before half of the delay. It expects the reply on the sender socket, not on the multicast group.
        """
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        last_offer_ts = wait_for_main_phase_anchor_offer(tc8_host_ip)

        half_delay_secs = (CYCLIC_OFFER_DELAY_MS / 2.0) / 1000.0
        send_sock = open_sender_socket(tc8_tester_ip)
        try:
            elapsed_secs = time.monotonic() - last_offer_ts
            assert elapsed_secs < half_delay_secs, (
                "FindService sent too late for the new-offer case "
                f"({elapsed_secs * 1000:.0f} ms after the last offer, half cyclic_offer_delay is "
                f"{half_delay_secs * 1000:.0f} ms)"
            )

            send_find_service(
                send_sock,
                (tc8_dut_ip, SD_PORT),
                service_id=SERVICE_ID,
                instance_id=INSTANCE_ID,
                major_version=MAJOR_VERSION,
                flag_unicast=True,  # Precondition: Unicast Flag = 1
            )

            unicast_reply = capture_unicast_sd_entries(
                send_sock,
                filter_types=(SdEntryType.OfferService,),
                timeout_secs=_UNICAST_REPLY_WINDOW_SECS,
            )
            matching = [e for e in unicast_reply if e.service_id == SERVICE_ID]
            assert matching, (
                "No unicast OfferService received within "
                f"{_UNICAST_REPLY_WINDOW_SECS:.1f} s of a Unicast-Flag=1 FindService after a new offer"
            )
        finally:
            send_sock.close()

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_cyclic_timing"],
        partially_verifies=[
            "AS__PRS_SOMEIPSD_00417",
            "AS__PRS_SOMEIPSD_00843",
            "SOMEIPSRV_SD_BEHAVIOR_04",
        ],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_sd_behavior_04_multicast_findservice_timing(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        tc8_tester_ip: str,
    ) -> None:
        """A multicast FindService (Unicast Flag 0) causes a multicast OfferService.

        The DUT must answer a FindService to the SD multicast group with a multicast OfferService.
        The test captures the response on the SD multicast socket, not on the unicast sender socket.
        The response must arrive in ``cyclic_offer_delay * 1.5`` after the FindService.

        On loopback, the multicast response arrives on the socket that joined the SD multicast
        group (``open_multicast_socket``). The test opens the sender socket and the listener socket
        first, so that the multicast socket listens when the FindService goes out.
        """
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        send_sock = open_sender_socket(tc8_tester_ip)
        mc_sock = open_multicast_socket(tc8_host_ip)
        try:
            t0 = time.monotonic()
            send_find_service(
                send_sock,
                (SD_MULTICAST_ADDR, SD_PORT),
                service_id=SERVICE_ID,
                instance_id=0xFFFF,
                major_version=0xFF,
                flag_unicast=False,
            )

            # Listen on the multicast socket for the OfferService response of the DUT.
            # The response can be the next cyclic offer or a triggered offer.
            # The first offer must arrive in cyclic_offer_delay * 1.5.
            _max_allowed_secs = (CYCLIC_OFFER_DELAY_MS * 1.5) / 1000.0
            deadline = time.monotonic() + _max_allowed_secs
            matching = []
            while time.monotonic() < deadline and not matching:
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    break
                mc_sock.settimeout(min(remaining, 0.5))
                try:
                    data, _ = mc_sock.recvfrom(65535)
                except socket.timeout:
                    continue
                parsed = parse_sd_message(data)
                if parsed is None:
                    continue
                for entry in parsed[1].entries:
                    if entry.sd_type == SdEntryType.OfferService and entry.service_id == SERVICE_ID:
                        matching.append(entry)
            t1 = time.monotonic()

            assert matching, (
                "No multicast OfferService received after "
                f"multicast FindService within {_max_allowed_secs:.1f} s "
                f"(cyclic_offer_delay * 1.5 = {CYCLIC_OFFER_DELAY_MS * 1.5:.0f} ms)"
            )
            elapsed_ms = (t1 - t0) * 1000.0
            max_allowed_ms = CYCLIC_OFFER_DELAY_MS * 1.5
            assert elapsed_ms <= max_allowed_ms, (
                f"OfferService arrived in {elapsed_ms:.0f} ms, "
                f"more than cyclic_offer_delay * 1.5 = {max_allowed_ms:.0f} ms"
            )
        finally:
            send_sock.close()
            mc_sock.close()


# ---------------------------------------------------------------------------
# Multiple subscriptions and subscription lifecycle edge cases
# ---------------------------------------------------------------------------


class TestSDSubscribeLifecycleAdvanced:
    """Multiple subscriptions and subscription lifecycle edge cases."""

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_sub_lifecycle"],
        partially_verifies=["AS__PRS_SOMEIPSD_00266", "SOMEIP_ETS_088"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_ets_088_two_subscribes_same_session(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        tc8_tester_ip: str,
    ) -> None:
        """One SD message with three SubscribeEventgroup entries gets an Ack for each entry.

        The test sends one datagram with three SubscribeEventgroup entries. It expects a
        SubscribeEventgroupAck for each entry. The deployment declares three eventgroups:
        ``EVENTGROUP_ID``, ``MULTICAST_EVENTGROUP_ID`` and ``_STATIC_FIELD_EVENTGROUP_ID``.
        """
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        sockets_to_close: list[socket.socket] = []
        try:
            sock = open_sender_socket(tc8_tester_ip)
            sockets_to_close.append(sock)
            eg1_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            sockets_to_close.append(eg1_sock)
            eg1_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            eg1_sock.bind((tc8_tester_ip, 0))
            eg2_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            sockets_to_close.append(eg2_sock)
            eg2_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            eg2_sock.bind((tc8_tester_ip, 0))
            eg3_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            sockets_to_close.append(eg3_sock)
            eg3_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            eg3_sock.bind((tc8_tester_ip, 0))

            eg1_port = eg1_sock.getsockname()[1]
            eg2_port = eg2_sock.getsockname()[1]
            eg3_port = eg3_sock.getsockname()[1]

            def _build_entries() -> list[SdEntry]:
                return [
                    build_subscribe_entry(
                        SERVICE_ID, INSTANCE_ID, EVENTGROUP_ID, MAJOR_VERSION, tc8_tester_ip, eg1_port
                    ),
                    build_subscribe_entry(
                        SERVICE_ID, INSTANCE_ID, MULTICAST_EVENTGROUP_ID, MAJOR_VERSION, tc8_tester_ip, eg2_port
                    ),
                    build_subscribe_entry(
                        SERVICE_ID,
                        INSTANCE_ID,
                        _STATIC_FIELD_EVENTGROUP_ID,
                        MAJOR_VERSION,
                        tc8_tester_ip,
                        eg3_port,
                    ),
                ]

            def _send_all() -> None:
                send_sd_entries(sock, (tc8_dut_ip, SD_PORT), _build_entries())

            _send_all()

            entries = capture_unicast_sd_entries(
                sock,
                filter_types=(SdEntryType.SubscribeAck,),
                timeout_secs=6.0,
                resend=_send_all,
            )
            acks_eg1 = [e for e in entries if e.eventgroup_id == EVENTGROUP_ID and e.ttl > 0]
            acks_eg2 = [e for e in entries if e.eventgroup_id == MULTICAST_EVENTGROUP_ID and e.ttl > 0]
            acks_eg3 = [e for e in entries if e.eventgroup_id == _STATIC_FIELD_EVENTGROUP_ID and e.ttl > 0]
            assert acks_eg1, f"No SubscribeAck received for eventgroup 0x{EVENTGROUP_ID:04x}"
            assert acks_eg2, f"No SubscribeAck received for eventgroup 0x{MULTICAST_EVENTGROUP_ID:04x}"
            assert acks_eg3, f"No SubscribeAck received for eventgroup 0x{_STATIC_FIELD_EVENTGROUP_ID:04x}"
        finally:
            for s in sockets_to_close:
                s.close()

    @add_test_properties(
        partially_verifies=["AS__PRS_SOMEIPSD_00466", "SOMEIP_ETS_092"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_ets_092_ttl_zero_stop_subscribe_no_nack(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        tc8_tester_ip: str,
    ) -> None:
        """A SubscribeEventgroup with TTL=0 is a StopSubscribe. The DUT sends no NAck.

        Partial: the test does not check for a positive SubscribeEventgroupAck to the TTL=0 entry.

        A subscribe entry with TTL=0 is a StopSubscribeEventgroup. The DUT must not send
        a SubscribeAck (positive or negative) as a response.
        """
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        sock = open_sender_socket(tc8_tester_ip)
        try:
            sender_port = sock.getsockname()[1]
            send_subscribe_eventgroup(
                sock,
                (tc8_dut_ip, SD_PORT),
                SERVICE_ID,
                INSTANCE_ID,
                EVENTGROUP_ID,
                MAJOR_VERSION,
                subscriber_ip=tc8_tester_ip,
                subscriber_port=sender_port,
                ttl=0,
            )
            # Do not send again. A StopSubscribe must not cause an ACK.
            entries = capture_unicast_sd_entries(
                sock,
                filter_types=(SdEntryType.SubscribeAck,),
                timeout_secs=2.0,
            )
            # The DUT must not send a NAck (TTL=0 Ack) for a StopSubscribe.
            nacks = [e for e in entries if e.eventgroup_id == EVENTGROUP_ID and e.ttl == 0]
            assert not nacks, (
                f"DUT sent NAck (TTL=0 SubscribeAck) in response to "
                f"StopSubscribeEventgroup (TTL=0). Got {len(nacks)} NAck(s). "
                "A StopSubscribe must not cause an acknowledgement."
            )
        finally:
            sock.close()

    @add_test_properties(
        fully_verifies=["AS__PRS_SOMEIPSD_00393"],
        partially_verifies=["AS__PRS_SOMEIPSD_00131", "AS__PRS_SOMEIPSD_00566", "SOMEIP_ETS_096"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_ets_096_subscribe_tcp_endpoint_no_connection(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        tc8_tester_ip: str,
    ) -> None:
        """A SubscribeEventgroup names a TCP endpoint, but no TCP connection exists.

        The test opens no socket on the named port. The DUT must reject the subscription with a
        SubscribeEventgroupNAck. It must not accept the subscription or wait for a connection.
        """
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        sock = open_sender_socket(tc8_tester_ip)
        try:
            sender_port = sock.getsockname()[1]

            def _send() -> None:
                send_subscribe_eventgroup(
                    sock,
                    (tc8_dut_ip, SD_PORT),
                    SERVICE_ID,
                    INSTANCE_ID,
                    EVENTGROUP_ID,
                    MAJOR_VERSION,
                    subscriber_ip=tc8_tester_ip,
                    subscriber_port=sender_port,
                    l4proto=L4Proto.TCP,
                )

            _send()
            entries = capture_unicast_sd_entries(
                sock,
                filter_types=(SdEntryType.SubscribeAck,),
                timeout_secs=5.0,
                resend=_send,
            )
            nacks = [e for e in entries if e.eventgroup_id == EVENTGROUP_ID and e.ttl == 0]
            assert nacks, (
                "No SubscribeEventgroupNAck (TTL=0) received for a TCP-endpoint subscribe "
                "with no TCP connection established"
            )
        finally:
            sock.close()

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_sub_lifecycle"],
        partially_verifies=["AS__PRS_SOMEIPSD_00390", "AS__PRS_SOMEIPSD_00466"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_subscribe_accepted_without_prior_rpc(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        tc8_tester_ip: str,
    ) -> None:
        """The DUT accepts a SubscribeEventgroup without a method call before it.

        A server must not require a method call from the client before it accepts
        an eventgroup subscription. The test expects a positive ACK (TTL > 0).
        """
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        sock = open_sender_socket(tc8_tester_ip)
        try:
            sender_port = sock.getsockname()[1]

            def _send() -> None:
                send_subscribe_eventgroup(
                    sock,
                    (tc8_dut_ip, SD_PORT),
                    SERVICE_ID,
                    INSTANCE_ID,
                    EVENTGROUP_ID,
                    MAJOR_VERSION,
                    subscriber_ip=tc8_tester_ip,
                    subscriber_port=sender_port,
                )

            _send()
            entries = capture_unicast_sd_entries(
                sock,
                filter_types=(SdEntryType.SubscribeAck,),
                timeout_secs=5.0,
                resend=_send,
            )
            acks = [e for e in entries if e.eventgroup_id == EVENTGROUP_ID and e.ttl > 0]
            assert acks, (
                "No SubscribeAck (TTL>0) received without a prior method call. "
                "The server must accept subscriptions without conditions."
            )
        finally:
            sock.close()

    @add_test_properties(
        partially_verifies=[
            "AS__PRS_SOMEIPSD_00263",
            "AS__PRS_SOMEIPSD_00390",
            "AS__PRS_SOMEIPSD_00466",
            "SOMEIP_ETS_107",
        ],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_ets_107_find_service_and_subscribe_processed_independently(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        tc8_tester_ip: str,
    ) -> None:
        """A StopSubscribe and a new Subscribe after it make a new subscription.

        Partial: the 500 ms cyclic notifications of TestField also satisfy the initial-event check.

        First, the test subscribes and makes sure that the DUT sends an Ack and an initial
        notification. Then it sends one SD message with a StopSubscribeEventgroup (TTL=0) entry
        and a new SubscribeEventgroup (TTL>0) entry for the same eventgroup, in that order.
        The DUT must process the stop before the new subscribe and send a SubscribeEventgroupAck
        for the new subscription. This test checks the entry order in one message.
        """
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        sockets_to_close: list[socket.socket] = []
        try:
            sd_sock = open_sender_socket(tc8_tester_ip)
            sockets_to_close.append(sd_sock)
            notif_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            sockets_to_close.append(notif_sock)
            notif_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            notif_sock.bind((tc8_tester_ip, 0))
            notif_port: int = notif_sock.getsockname()[1]

            # Step 1: subscribe and make sure that the DUT sends an ack and a notification.
            def _send_initial_sub() -> None:
                send_subscribe_eventgroup(
                    sd_sock,
                    (tc8_dut_ip, SD_PORT),
                    SERVICE_ID,
                    INSTANCE_ID,
                    EVENTGROUP_ID,
                    MAJOR_VERSION,
                    subscriber_ip=tc8_tester_ip,
                    subscriber_port=notif_port,
                )

            _send_initial_sub()
            initial_acks = capture_unicast_sd_entries(
                sd_sock,
                filter_types=(SdEntryType.SubscribeAck,),
                timeout_secs=5.0,
                resend=_send_initial_sub,
            )
            assert any(e.eventgroup_id == EVENTGROUP_ID and e.ttl > 0 for e in initial_acks), (
                "Precondition failed: no initial SubscribeEventgroupAck received"
            )
            initial_notifs = capture_some_ip_messages(notif_sock, SERVICE_ID, timeout_secs=4.0)
            assert initial_notifs, "Precondition failed: no initial notifications received"

            # Step 2: one SD message with a StopSubscribe (TTL=0) and then a new Subscribe (TTL>0)
            # for the same eventgroup, in that order.
            stop_entry = build_subscribe_entry(
                SERVICE_ID, INSTANCE_ID, EVENTGROUP_ID, MAJOR_VERSION, tc8_tester_ip, notif_port, ttl=0
            )
            resubscribe_entry = build_subscribe_entry(
                SERVICE_ID, INSTANCE_ID, EVENTGROUP_ID, MAJOR_VERSION, tc8_tester_ip, notif_port, ttl=3
            )

            def _send_stop_then_resubscribe() -> None:
                send_sd_entries(sd_sock, (tc8_dut_ip, SD_PORT), [stop_entry, resubscribe_entry])

            _send_stop_then_resubscribe()
            entries = capture_unicast_sd_entries(
                sd_sock,
                filter_types=(SdEntryType.SubscribeAck,),
                timeout_secs=6.0,
                resend=_send_stop_then_resubscribe,
            )
            resubscribe_acks = [e for e in entries if e.eventgroup_id == EVENTGROUP_ID and e.ttl > 0]
            assert resubscribe_acks, (
                "No SubscribeEventgroupAck received for the re-subscription "
                "that followed the StopSubscribe entry in the same SD message"
            )
            resubscribe_notifs = capture_some_ip_messages(notif_sock, SERVICE_ID, timeout_secs=4.0)
            assert resubscribe_notifs, (
                "No notifications received after the re-subscription that followed the "
                "StopSubscribe entry in the same SD message"
            )
        finally:
            for s in sockets_to_close:
                s.close()

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_sub_lifecycle"],
        partially_verifies=["AS__PRS_SOMEIPSD_00484", "SOMEIP_ETS_120"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_ets_120_subscribe_endpoint_ip_matches_tester(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        tc8_tester_ip: str,
    ) -> None:
        """The SubscribeEventgroupAck and the initial fields go to different destinations.

        Variant: the Endpoint Option and the SD source have different ports only.
        ``tc8_network`` has no tester address other than ``tc8_tester_ip``.
        The Endpoint Option names the port of a different socket, so the two checks use different destinations:

        (a) The SubscribeEventgroupAck arrives at the SD source, which is the socket that sent the SubscribeEventgroup.
        (b) The initial notifications arrive at the IP and port in the Endpoint Option of the SubscribeEventgroup.
        """
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        sockets_to_close: list[socket.socket] = []
        try:
            sd_sock = open_sender_socket(tc8_tester_ip)
            sockets_to_close.append(sd_sock)
            notif_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            sockets_to_close.append(notif_sock)
            notif_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            notif_sock.bind((tc8_tester_ip, 0))
            notif_port: int = notif_sock.getsockname()[1]

            def _send() -> None:
                send_subscribe_eventgroup(
                    sd_sock,
                    (tc8_dut_ip, SD_PORT),
                    SERVICE_ID,
                    INSTANCE_ID,
                    EVENTGROUP_ID,
                    MAJOR_VERSION,
                    subscriber_ip=tc8_tester_ip,
                    subscriber_port=notif_port,
                )

            _send()

            # (a) The Ack goes to the SD source, not to the address of the Endpoint Option.
            entries = capture_unicast_sd_entries(
                sd_sock,
                filter_types=(SdEntryType.SubscribeAck,),
                timeout_secs=5.0,
                resend=_send,
            )
            acks = [e for e in entries if e.eventgroup_id == EVENTGROUP_ID and e.ttl > 0]
            assert acks, f"No SubscribeAck received at the SD source {tc8_tester_ip}:{sd_sock.getsockname()[1]}"

            # (b) The initial notifications go to the address and port of the Endpoint Option.
            initial_notifs = capture_some_ip_messages(notif_sock, SERVICE_ID, timeout_secs=4.0)
            assert initial_notifs, (
                f"No initial notification traffic received at the "
                f"SubscribeEventgroup Endpoint Option address {tc8_tester_ip}:{notif_port}"
            )
        finally:
            for s in sockets_to_close:
                s.close()

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_offer_format", "AS__PRS_SOMEIPSD_00251"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_sd_header_interface_version_is_one(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
    ) -> None:
        """The SD outer header has the fixed Method ID, versions, message type and return code.

        The Length field must also agree with the number of bytes after it in the datagram.
        """
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        mc_sock = open_multicast_socket(tc8_host_ip)
        try:
            deadline = time.monotonic() + 6.0
            found: list[tuple[SomeipResponse, int]] = []
            while time.monotonic() < deadline and not found:
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    break
                mc_sock.settimeout(min(remaining, 1.0))
                try:
                    data, _ = mc_sock.recvfrom(65535)
                except socket.timeout:
                    continue
                try:
                    someip_msg = parse_someip(data)
                except MalformedSomeip:
                    continue
                if someip_msg.service_id == SD_SERVICE_ID:
                    found.append((someip_msg, len(data)))
        finally:
            mc_sock.close()

        assert found, "No SOME/IP-SD messages captured within 6 s"
        sd_msg, datagram_len = found[0]
        assert sd_msg.method_id == 0x8100, f"SD method_id = 0x{sd_msg.method_id:04x}, expected 0x8100"
        assert sd_msg.protocol_version == 1, f"SD protocol_version = {sd_msg.protocol_version}, expected 0x01"
        assert sd_msg.interface_version == 1, f"SD interface_version = {sd_msg.interface_version}, expected 0x01"
        assert sd_msg.message_type == MessageType.NOTIFICATION, (
            f"SD message_type = {sd_msg.message_type!r}, expected 0x02"
        )
        assert sd_msg.return_code == ReturnCode.E_OK, f"SD return_code = {sd_msg.return_code!r}, expected 0x00"
        assert sd_msg.length == datagram_len - 8, f"SD length = {sd_msg.length}, expected {datagram_len - 8}"

    @add_test_properties(
        partially_verifies=[
            "AS__PRS_SOMEIPSD_00390",
            "AS__PRS_SOMEIPSD_00453",
            "AS__PRS_SOMEIPSD_00466",
            "SOMEIP_ETS_155",
        ],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_ets_155_resubscribe_after_stop(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        tc8_tester_ip: str,
    ) -> None:
        """A new Subscribe after a StopSubscribe gets a new ACK, and the events start again.

        Partial: the 500 ms cyclic notifications of TestField also satisfy the initial-field check after the new subscribe.

        Sequence: Subscribe -> ACK -> StopSubscribe (TTL=0) -> Subscribe -> ACK -> notification.
        The DUT must accept the second subscription and send events again.
        """
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        sd_sock = open_sender_socket(tc8_tester_ip)
        notif_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        notif_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        notif_sock.bind((tc8_tester_ip, 0))
        notif_port: int = notif_sock.getsockname()[1]

        try:

            def _subscribe() -> None:
                send_subscribe_eventgroup(
                    sd_sock,
                    (tc8_dut_ip, SD_PORT),
                    SERVICE_ID,
                    INSTANCE_ID,
                    EVENTGROUP_ID,
                    MAJOR_VERSION,
                    subscriber_ip=tc8_tester_ip,
                    subscriber_port=notif_port,
                )

            # Step 1: First subscribe.
            _subscribe()
            acks1 = capture_unicast_sd_entries(
                sd_sock,
                filter_types=(SdEntryType.SubscribeAck,),
                timeout_secs=5.0,
                resend=_subscribe,
            )
            assert any(e.eventgroup_id == EVENTGROUP_ID and e.ttl > 0 for e in acks1), (
                "Precondition failed: no initial SubscribeAck received"
            )
            # Read the notifications before the stop. This makes sure that the events started. It also empties
            # notif_sock, so that old data in the receive buffer cannot make the last capture pass.
            initial_notifs = capture_some_ip_messages(notif_sock, SERVICE_ID, timeout_secs=4.0)
            assert initial_notifs, "Precondition failed: no initial notifications received"

            # Step 2: Stop the subscription.
            send_subscribe_eventgroup(
                sd_sock,
                (tc8_dut_ip, SD_PORT),
                SERVICE_ID,
                INSTANCE_ID,
                EVENTGROUP_ID,
                MAJOR_VERSION,
                subscriber_ip=tc8_tester_ip,
                subscriber_port=notif_port,
                ttl=0,
            )
            time.sleep(0.5)  # Give the DUT time to process the StopSubscribe.
            # Discard notifications that the DUT sent before it processed the StopSubscribe.
            # Thus the test does not count them as notifications after the new subscribe.
            capture_some_ip_messages(notif_sock, SERVICE_ID, timeout_secs=0.5)

            # Step 3: Subscribe again. The DUT must accept the subscription.
            _subscribe()
            acks2 = capture_unicast_sd_entries(
                sd_sock,
                filter_types=(SdEntryType.SubscribeAck,),
                timeout_secs=5.0,
                resend=_subscribe,
            )
            assert any(e.eventgroup_id == EVENTGROUP_ID and e.ttl > 0 for e in acks2), (
                "No SubscribeAck received after re-subscribe following StopSubscribe"
            )
            resumed_notifs = capture_some_ip_messages(notif_sock, SERVICE_ID, timeout_secs=4.0)
            assert resumed_notifs, "No notifications received after re-subscribe following StopSubscribe"
        finally:
            sd_sock.close()
            notif_sock.close()

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_ttl_expiry"],
        partially_verifies=["AS__PRS_SOMEIPSD_00466", "SOMEIP_ETS_095"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_ets_095_subscribe_ttl_expires_no_events(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        tc8_tester_ip: str,
    ) -> None:
        """No NOTIFICATION messages arrive after the subscription TTL expires.

        When the subscription TTL expires without a renewal, the server must stop
        the event notifications to that subscriber.

        Test sequence:
          1. Subscribe with TTL=3 (approximately 3 s).
          2. Wait until the TTL expires. Do not renew the subscription.
          3. Make sure that no SOME/IP NOTIFICATION messages arrive after the expiry.
        """
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        # Use TTL=3. TTL=1 is too short for a reliable expiry before the next notification cycle.
        _TTL_SECS = 3

        sd_sock = open_sender_socket(tc8_tester_ip)
        notif_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        notif_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        notif_sock.bind((tc8_tester_ip, 0))
        notif_port: int = notif_sock.getsockname()[1]

        try:

            def _send_sub_ttl() -> None:
                send_subscribe_eventgroup(
                    sd_sock,
                    (tc8_dut_ip, SD_PORT),
                    SERVICE_ID,
                    INSTANCE_ID,
                    EVENTGROUP_ID,
                    MAJOR_VERSION,
                    subscriber_ip=tc8_tester_ip,
                    subscriber_port=notif_port,
                    ttl=_TTL_SECS,
                )

            _send_sub_ttl()
            acks = capture_unicast_sd_entries(
                sd_sock,
                filter_types=(SdEntryType.SubscribeAck,),
                timeout_secs=5.0,
                resend=_send_sub_ttl,
            )
            assert any(e.eventgroup_id == EVENTGROUP_ID and e.ttl > 0 for e in acks), (
                "Precondition failed: no SubscribeEventgroupAck received"
            )

            # Read the notifications of the active subscription,
            # so that they do not arrive in the window after the expiry.
            pre_expiry = capture_some_ip_messages(notif_sock, SERVICE_ID, timeout_secs=4.0)
            assert pre_expiry, "No notifications received before TTL expiry"

            # Wait until the TTL expires (TTL + 2 s margin). Do not renew.
            time.sleep(_TTL_SECS + 2)

            # Make sure that no notifications arrive after the TTL expires.
            post_expiry = capture_some_ip_messages(notif_sock, SERVICE_ID, timeout_secs=3.0)
            assert not post_expiry, (
                f"{len(post_expiry)} NOTIFICATION(s) received after "
                f"subscription TTL expired (TTL={_TTL_SECS} s + 2 s margin). "
                "The server must stop the events when the subscription TTL expires."
            )
        finally:
            sd_sock.close()
            notif_sock.close()


# ---------------------------------------------------------------------------
# FindService and offer lifecycle, advanced cases
# ---------------------------------------------------------------------------


class TestSDFindServiceAdvanced:
    """FindService and offer lifecycle, advanced cases."""

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_offer_format"],
        partially_verifies=["AS__PRS_SOMEIPSD_00154", "AS__PRS_SOMEIPSD_00157", "SOMEIP_ETS_091"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_ets_091_session_id_increments(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
    ) -> None:
        """The session_id of each SD message is one more than the session_id of the message before it.

        The test captures at least 2 OfferService packets from the DUT. The session_id of each
        packet must be one more than the session_id of the packet before it.
        Each captured packet must also have client_id 0x0000.
        The DUT sends a cyclic offer every 2000 ms. The test waits up to 8 s to capture 3 offers.
        """
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        mc_sock = open_multicast_socket(tc8_host_ip)
        try:
            session_ids: list[int] = []
            deadline = time.monotonic() + 8.0
            while time.monotonic() < deadline and len(session_ids) < 3:
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    break
                mc_sock.settimeout(min(remaining, 1.0))
                try:
                    data, _ = mc_sock.recvfrom(65535)
                except socket.timeout:
                    continue
                parsed = parse_sd_message(data)
                if parsed is None:
                    continue
                someip_msg, sd_msg = parsed
                has_offer = any(
                    e.sd_type == SdEntryType.OfferService and e.service_id == SERVICE_ID for e in sd_msg.entries
                )
                if has_offer:
                    assert someip_msg.client_id == 0x0000, (
                        f"SD client_id must be 0x0000; got 0x{someip_msg.client_id:04x}"
                    )
                    session_ids.append(someip_msg.session_id)
        finally:
            mc_sock.close()

        assert len(session_ids) >= 2, (
            f"Only {len(session_ids)} OfferService packet(s) captured "
            "(need at least 2 to check session_id monotonicity)"
        )
        for i in range(len(session_ids) - 1):
            current = session_ids[i]
            nxt = session_ids[i + 1]
            # The 16-bit session_id goes from 0xFFFF back to 1.
            is_increment = nxt == (current % 0xFFFF) + 1
            assert is_increment, f"session_id did not increment monotonically: {current:#06x} -> {nxt:#06x}"

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_sub_lifecycle"],
        partially_verifies=["AS__PRS_SOMEIPSD_00464", "SOMEIP_ETS_121"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_ets_121_initial_event_sent_after_subscribe(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        tc8_tester_ip: str,
    ) -> None:
        """After the SubscribeEventgroup ACK, the DUT sends the initial field value as an event.

        Partial: the 500 ms cyclic notifications of TestField also satisfy the initial-field check.
        Eventgroup 0x4455 has TestField. The DUT sends a notification for it every 500 ms.
        """
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        sd_sock = open_sender_socket(tc8_tester_ip)
        notif_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        notif_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        notif_sock.bind((tc8_tester_ip, 0))
        notif_port: int = notif_sock.getsockname()[1]

        try:

            def _subscribe() -> None:
                send_subscribe_eventgroup(
                    sd_sock,
                    (tc8_dut_ip, SD_PORT),
                    SERVICE_ID,
                    INSTANCE_ID,
                    EVENTGROUP_ID,
                    MAJOR_VERSION,
                    subscriber_ip=tc8_tester_ip,
                    subscriber_port=notif_port,
                )

            _subscribe()
            acks = capture_unicast_sd_entries(
                sd_sock,
                filter_types=(SdEntryType.SubscribeAck,),
                timeout_secs=5.0,
                resend=_subscribe,
            )
            assert any(e.eventgroup_id == EVENTGROUP_ID and e.ttl > 0 for e in acks), (
                "Precondition failed: no SubscribeAck received"
            )

            # Expect at least one notification (the initial field value and the cyclic updates).
            notifs = capture_some_ip_messages(notif_sock, SERVICE_ID, timeout_secs=5.0)
            assert notifs, (
                "No SOME/IP notifications received after subscribe. "
                "The DUT must send the initial field value when it accepts a subscription."
            )
        finally:
            sd_sock.close()
            notif_sock.close()

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_offer_format"],
        partially_verifies=["AS__PRS_SOMEIPSD_00350"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_server_sends_no_findservice(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        tc8_tester_ip: str,
    ) -> None:
        """The DUT (server) must not send FindService entries in the main phase.

        A SOME/IP server that offers its service must not send FindService SD entries.
        The test captures unicast SD entries for 5 s on the tester socket at SD_PORT.
        No entry from the DUT must be a FindService.
        """
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        sock = open_sender_socket(tc8_tester_ip)
        try:
            entries = capture_unicast_sd_entries(
                sock,
                filter_types=(SdEntryType.FindService,),
                timeout_secs=5.0,
            )
            assert not entries, (
                f"DUT sent {len(entries)} FindService SD entry/ies. "
                "A server in the main phase must not send FindService."
            )
        finally:
            sock.close()

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_find_response"],
        partially_verifies=["AS__PRS_SOMEIPSD_00422", "AS__PRS_SOMEIPSD_00825", "SOMEIP_ETS_128"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_ets_128_multicast_findservice_version_wildcard(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        tc8_tester_ip: str,
    ) -> None:
        """A multicast FindService with major=0xFF and minor=0xFFFFFFFF causes an OfferService.

        Variant: one entry has the two version wildcards. There are not two series with one wildcard each.

        The DUT must answer a FindService with wildcard versions to the SD multicast address
        with an OfferService.
        """
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        sock = open_sender_socket(tc8_tester_ip)
        try:

            def _send() -> None:
                send_find_service(
                    sock,
                    (SD_MULTICAST_ADDR, SD_PORT),
                    service_id=SERVICE_ID,
                    instance_id=0xFFFF,
                    major_version=0xFF,
                    minor_version=0xFFFFFFFF,
                )

            _send()
            entries = capture_unicast_sd_entries(
                sock,
                filter_types=(SdEntryType.OfferService,),
                timeout_secs=5.0,
                resend=_send,
            )
            matching = [e for e in entries if e.service_id == SERVICE_ID]
            assert matching, (
                "No OfferService received for multicast FindService "
                "with wildcard version (major=0xFF, minor=0xFFFFFFFF)"
            )
        finally:
            sock.close()

    @pytest.mark.xfail(strict=True, reason=_NO_UNICAST_OFFER_REPLY_WILDCARD_XFAIL_REASON)
    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_find_response"],
        partially_verifies=[
            "AS__PRS_SOMEIPSD_00422",
            "AS__PRS_SOMEIPSD_00500",
            "AS__PRS_SOMEIPSD_00825",
            "SOMEIP_ETS_130",
        ],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_ets_130_multicast_findservice_unicast_flag_clear(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        tc8_tester_ip: str,
    ) -> None:
        """A FindService with unicast_flag=0 must also get a unicast OfferService reply.

        The DUT answers with unicast, although the Unicast Flag is 0. The cyclic multicast offers do not count.
        The test listens only on the unicast socket of the sender, not on the multicast group.
        Thus an offer that arrives is a reply to this FindService, not a cyclic multicast offer (every 2 s).
        """
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        from someip_itf.tc8.sd_malformed import _find_service_entry_bytes, build_raw_sd_packet  # noqa: PLC0415

        sock = open_sender_socket(tc8_tester_ip)
        try:
            # SD flags: reboot bit (0x80) is 1, unicast bit (0x40) is 0 → 0x80
            entry_bytes = _find_service_entry_bytes(
                service_id=SERVICE_ID,
                instance_id=0xFFFF,
                major_version=0xFF,
                minor_version=0xFFFFFFFF,
            )
            pkt = build_raw_sd_packet(flags=0x80, entries_bytes=entry_bytes)

            def _send() -> None:
                sock.sendto(pkt, (SD_MULTICAST_ADDR, SD_PORT))

            _send()
            entries = capture_unicast_sd_entries(
                sock,
                filter_types=(SdEntryType.OfferService,),
                timeout_secs=5.0,
                resend=_send,
            )
            matching = [e for e in entries if e.service_id == SERVICE_ID]
            assert matching, "No unicast OfferService received after multicast FindService with unicast_flag=0"
        finally:
            sock.close()


# ---------------------------------------------------------------------------
# Tester reboot detection (server side)
# ---------------------------------------------------------------------------

#: Session IDs out of the range of the automatic counter of the other tests in this module.
#: That counter starts at 1 and stays small. Thus the reboot values of this test are
#: always different from the automatic values.
_REBOOT_TEST_INITIAL_SESSION_ID: int = 9001
_REBOOT_TEST_REBOOTED_SESSION_ID: int = 1


class TestSDRebootDetectionServerSide:
    """The DUT detects a reboot of the Tester and stops the events."""

    @add_test_properties(
        partially_verifies=["AS__PRS_SOMEIPSD_00258", "SOMEIP_ETS_094"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    @pytest.mark.xfail(strict=True, reason=_LGSOMEIP_9_XFAIL_REASON)
    def test_ets_094_reboot_detection_server_side(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
        tc8_tester_ip: str,
    ) -> None:
        """The DUT stops the events when it detects that the Tester rebooted.

        Partial: the test sends no FindService without the Reboot Flag before the FindService with the flag.

        The Tester subscribes and makes sure that notifications arrive. Then it sends a FindService with the
        Reboot Flag set and a Session ID lower than all Session IDs it used before. This tells the DUT
        that the sender restarted. The DUT must cancel the subscription and stop the notifications.
        """
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        sd_sock = open_sender_socket(tc8_tester_ip)
        notif_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        notif_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        notif_sock.bind((tc8_tester_ip, 0))
        notif_port: int = notif_sock.getsockname()[1]

        try:

            def _subscribe() -> None:
                send_subscribe_eventgroup(
                    sd_sock,
                    (tc8_dut_ip, SD_PORT),
                    SERVICE_ID,
                    INSTANCE_ID,
                    EVENTGROUP_ID,
                    MAJOR_VERSION,
                    subscriber_ip=tc8_tester_ip,
                    subscriber_port=notif_port,
                    # The TTL is longer than the test (ack wait and two notification captures below).
                    # The default ttl=3 can expire during the test. Then a normal TTL expiry can hide
                    # the reboot detection.
                    ttl=30,
                    session_id=_REBOOT_TEST_INITIAL_SESSION_ID,
                )

            _subscribe()
            acks = capture_unicast_sd_entries(
                sd_sock,
                filter_types=(SdEntryType.SubscribeAck,),
                timeout_secs=5.0,
                resend=_subscribe,
                # Return when the ack arrives. Do not wait the full timeout_secs.
                max_results=1,
            )
            assert any(e.eventgroup_id == EVENTGROUP_ID and e.ttl > 0 for e in acks), (
                "Precondition failed: no initial SubscribeEventgroupAck received"
            )
            initial_notifs = capture_some_ip_messages(notif_sock, SERVICE_ID, timeout_secs=4.0)
            assert initial_notifs, "Precondition failed: no initial notifications received"

            send_find_service(
                sd_sock,
                (tc8_dut_ip, SD_PORT),
                SERVICE_ID,
                session_id=_REBOOT_TEST_REBOOTED_SESSION_ID,
                flag_reboot=True,
            )

            post_reboot_notifs = capture_some_ip_messages(notif_sock, SERVICE_ID, timeout_secs=4.0)
            assert not post_reboot_notifs, (
                f"{len(post_reboot_notifs)} notification(s) received after a reboot-flagged "
                "FindService with a lower Session ID. The DUT must stop the events to the previous "
                "subscription when it detects that the sender rebooted"
            )
        finally:
            sd_sock.close()
            notif_sock.close()
