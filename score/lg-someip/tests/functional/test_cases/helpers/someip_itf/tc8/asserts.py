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
"""Assertion helpers for the TC8 conformance tests.

The helpers check SD entry fields, SOME/IP message headers and the SubscribeEventgroup replies of the DUT.
"""

import socket

from scapy.contrib.automotive.someip import SDOption_IP4_EndPoint

from someip_itf.codec import L4Proto, MessageType, ReturnCode, SdEntry, SdEntryType, SomeipResponse
from someip_itf.tc8.sd_sender import capture_unicast_sd_entries


def assert_sd_offer_entry(
    entry: SdEntry,
    expected_service_id: int,
    expected_instance_id: int,
    expected_major_version: int = 0x00,
    expected_minor_version: int = 0x00000000,
) -> None:
    """Assert that an SD OFFER entry has the expected fields."""
    # The entry type must be OfferService.
    assert entry.sd_type == SdEntryType.OfferService, f"expected OfferService entry, got {entry.sd_type.name}"

    # The service ID must agree with the configuration.
    assert entry.service_id == expected_service_id, (
        f"service_id mismatch: got 0x{entry.service_id:04x}, expected 0x{expected_service_id:04x}"
    )

    # The instance ID must agree with the configuration.
    assert entry.instance_id == expected_instance_id, (
        f"instance_id mismatch: got 0x{entry.instance_id:04x}, expected 0x{expected_instance_id:04x}"
    )

    # The major version must agree with the service definition.
    assert entry.major_version == expected_major_version, (
        f"major_version mismatch: got 0x{entry.major_version:02x}, expected 0x{expected_major_version:02x}"
    )

    # The minor version must agree with the service definition.
    assert entry.service_minor_version == expected_minor_version, (
        f"minor_version mismatch: got 0x{entry.service_minor_version:08x}, expected 0x{expected_minor_version:08x}"
    )

    # The TTL must be more than 0. TTL=0 is a StopOffer.
    assert entry.ttl > 0, f"OFFER TTL must be > 0; got {entry.ttl}"


def assert_valid_response(resp: SomeipResponse, req_service_id: int, req_method_id: int) -> None:
    """Assert that a SOME/IP RESPONSE has the correct header fields."""
    assert resp.protocol_version == 1, f"protocol_version mismatch: got {resp.protocol_version}, expected 1"
    assert resp.message_type == MessageType.RESPONSE, (
        f"message_type mismatch: got {resp.message_type}, expected RESPONSE (0x{MessageType.RESPONSE:02x})"
    )
    assert resp.service_id == req_service_id, (
        f"service_id mismatch: got 0x{resp.service_id:04x}, expected 0x{req_service_id:04x}"
    )
    assert resp.method_id == req_method_id, (
        f"method_id mismatch: got 0x{resp.method_id:04x}, expected 0x{req_method_id:04x}"
    )


def assert_return_code(resp: SomeipResponse, expected: ReturnCode) -> None:
    """Assert that a SOME/IP message has the expected return code."""
    assert resp.return_code == expected, (
        f"return_code mismatch: got 0x{resp.return_code:02x}, expected {expected.name} (0x{expected.value:02x})"
    )


def assert_session_echo(resp: SomeipResponse, expected_session_id: int) -> None:
    """Assert that the RESPONSE session_id is equal to the REQUEST session_id."""
    assert resp.session_id == expected_session_id, (
        f"session_id mismatch: got 0x{resp.session_id:04x}, expected 0x{expected_session_id:04x}"
    )


def assert_client_echo(resp: SomeipResponse, expected_client_id: int) -> None:
    """Assert that the RESPONSE client_id is equal to the REQUEST client_id."""
    assert resp.client_id == expected_client_id, (
        f"client_id mismatch: got 0x{resp.client_id:04x}, expected 0x{expected_client_id:04x}"
    )


def assert_offer_has_ipv4_endpoint_option(
    entry: SdEntry,
    expected_ip: str,
    expected_port: int,
) -> None:
    """Assert that the OfferService entry has an IPv4EndpointOption for UDP.

    A client uses this unicast endpoint to reach the service.
    The address and the port must agree with the DUT configuration.
    The length, the discardable flag and the two reserved fields must have their fixed values.
    """
    options = list(entry.options_1) + list(entry.options_2)
    ipv4_opts = [o for o in options if isinstance(o, SDOption_IP4_EndPoint)]
    udp_opts = [o for o in ipv4_opts if o.l4_proto == L4Proto.UDP]
    assert udp_opts, (
        "no IPv4EndpointOption with L4Proto=UDP found in OfferService entry options. "
        f"Entry has {len(options)} option(s): {options}"
    )
    opt = udp_opts[0]
    assert opt.len == 0x0009, f"endpoint option length 0x{opt.len:04x}, expected 0x0009"
    assert opt.res_hdr == 0x00, f"discardable flag/reserved bits 0x{opt.res_hdr:02x}, expected 0x00"
    assert opt.res_tail == 0x00, f"reserved byte 0x{opt.res_tail:02x}, expected 0x00"
    assert opt.addr == expected_ip, f"endpoint address mismatch: got {opt.addr}, expected {expected_ip}"
    assert opt.port == expected_port, f"endpoint port mismatch: got {opt.port}, expected {expected_port}"


def assert_offer_has_tcp_endpoint_option(
    entry: SdEntry,
    expected_ip: str,
    expected_port: int,
) -> None:
    """Assert that the OfferService entry has an IPv4EndpointOption with L4Proto=TCP.

    The entry must have a TCP endpoint option when the service has a reliable port.
    """
    options = list(entry.options_1) + list(entry.options_2)
    ipv4_opts = [o for o in options if isinstance(o, SDOption_IP4_EndPoint)]
    tcp_opts = [o for o in ipv4_opts if o.l4_proto == L4Proto.TCP]
    assert tcp_opts, f"no IPv4EndpointOption with L4Proto=TCP found in OfferService entry. Options present: {options}"
    opt = tcp_opts[0]
    assert opt.addr == expected_ip, f"TCP endpoint address mismatch: got {opt.addr}, expected {expected_ip}"
    assert opt.port == expected_port, f"TCP endpoint port mismatch: got {opt.port}, expected {expected_port}"


#: Time to wait for an unwanted SubscribeAck after a Subscribe to an ID that the DUT never offers.
NO_ACK_WINDOW_SECS: float = 2.0


def matching_subscribe_acks(
    sock: socket.socket,
    service_id: int,
    instance_id: int,
    eventgroup_id: int,
    timeout_secs: float = NO_ACK_WINDOW_SECS,
) -> list:
    """Return the SubscribeAck-type entries for the given IDs that arrive within *timeout_secs*."""
    entries = capture_unicast_sd_entries(sock, filter_types=(SdEntryType.SubscribeAck,), timeout_secs=timeout_secs)
    return [
        e
        for e in entries
        if e.service_id == service_id and e.instance_id == instance_id and e.eventgroup_id == eventgroup_id
    ]


def assert_no_subscribe_ack(
    sock: socket.socket,
    service_id: int,
    instance_id: int,
    eventgroup_id: int,
    timeout_secs: float = NO_ACK_WINDOW_SECS,
) -> None:
    """Assert that no SubscribeEventgroupAck (TTL > 0) for the given IDs arrives within *timeout_secs*.

    A NAck (TTL=0) is not an error. For an eventgroup that the DUT does not offer, the DUT can send a NAck
    or discard the Subscribe. Only an accepting Ack is an error.
    """
    matching = matching_subscribe_acks(sock, service_id, instance_id, eventgroup_id, timeout_secs)
    acks = [e for e in matching if e.ttl > 0]
    assert not acks, (
        f"Unexpected SubscribeEventgroupAck for svc=0x{service_id:04x} inst=0x{instance_id:04x} "
        f"evtgrp=0x{eventgroup_id:04x}, which the DUT does not offer"
    )


def assert_nack_received(
    sock: socket.socket,
    service_id: int,
    instance_id: int,
    eventgroup_id: int,
    timeout_secs: float = NO_ACK_WINDOW_SECS,
) -> None:
    """Assert that an explicit SubscribeEventgroupNAck (TTL=0) for the given IDs arrives within *timeout_secs*.

    Some ETS cases require an explicit NAck. For these cases, no reply is an error,
    because a DUT that only drops the malformed Subscribe does not reject it.
    """
    matching = matching_subscribe_acks(sock, service_id, instance_id, eventgroup_id, timeout_secs)
    nacks = [e for e in matching if e.ttl == 0]
    assert nacks, (
        f"Expected an explicit SubscribeEventgroupNAck (TTL=0) for svc=0x{service_id:04x} "
        f"inst=0x{instance_id:04x} evtgrp=0x{eventgroup_id:04x}; none received within {timeout_secs:.1f}s"
    )


def assert_ack_or_silent(
    sock: socket.socket,
    service_id: int,
    instance_id: int,
    eventgroup_id: int,
    timeout_secs: float = NO_ACK_WINDOW_SECS,
) -> None:
    """Assert that no SubscribeEventgroupNAck (TTL=0) for the given IDs arrives.

    An accepting Ack (TTL > 0) or no reply is correct. This is the opposite of `assert_no_subscribe_ack`.
    Some cases, for example an options array that is too large but can be parsed, permit all replies but a NAck.
    """
    matching = matching_subscribe_acks(sock, service_id, instance_id, eventgroup_id, timeout_secs)
    nacks = [e for e in matching if e.ttl == 0]
    assert not nacks, (
        f"Unexpected SubscribeEventgroupNAck for svc=0x{service_id:04x} inst=0x{instance_id:04x} "
        f"evtgrp=0x{eventgroup_id:04x}; an accepting Ack or silence were the only compliant outcomes"
    )


def assert_ack_received(
    sock: socket.socket,
    service_id: int,
    instance_id: int,
    eventgroup_id: int,
    timeout_secs: float = NO_ACK_WINDOW_SECS,
) -> None:
    """Assert that an accepting SubscribeEventgroupAck (TTL > 0) for the given IDs arrives within *timeout_secs*."""
    matching = matching_subscribe_acks(sock, service_id, instance_id, eventgroup_id, timeout_secs)
    acks = [e for e in matching if e.ttl > 0]
    assert acks, (
        f"Expected an accepting SubscribeEventgroupAck (TTL>0) for svc=0x{service_id:04x} "
        f"inst=0x{instance_id:04x} evtgrp=0x{eventgroup_id:04x}; none received within {timeout_secs:.1f}s"
    )


def assert_no_subscribe_reply(
    sock: socket.socket,
    service_id: int,
    instance_id: int,
    eventgroup_id: int,
    timeout_secs: float = NO_ACK_WINDOW_SECS,
) -> None:
    """Assert that no SubscribeEventgroupAck and no NAck for the given IDs arrives within *timeout_secs*.

    The DUT never answers a StopSubscribeEventgroup.
    The StopSubscribe transitions of the server state machines send nothing.
    """
    matching = matching_subscribe_acks(sock, service_id, instance_id, eventgroup_id, timeout_secs)
    assert not matching, (
        f"Unexpected SubscribeEventgroupAck/NAck (TTL={[e.ttl for e in matching]}) for a StopSubscribe "
        f"svc=0x{service_id:04x} inst=0x{instance_id:04x} evtgrp=0x{eventgroup_id:04x}"
    )


def assert_no_sd_reply(sock: socket.socket, timeout_secs: float = NO_ACK_WINDOW_SECS) -> None:
    """Assert that no unicast SD entry of any type reaches *sock* within *timeout_secs*.

    A DUT that does not run also sends nothing. Thus, use this check together with `verify_dut_alive`.
    """
    stray = capture_unicast_sd_entries(sock, filter_types=None, timeout_secs=timeout_secs)
    assert not stray, f"Expected the DUT to discard the message; got {len(stray)} SD entrie(s): {stray}"
