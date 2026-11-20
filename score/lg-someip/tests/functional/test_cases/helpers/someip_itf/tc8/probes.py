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

"""Capture and request helpers that the TC8 tests use with Tc8Interface, in addition to the SD sender helpers."""

from __future__ import annotations

import socket
import time
from typing import Dict, Tuple

import pytest
from scapy.contrib.automotive.someip import SDOption_IP4_EndPoint, SDOption_IP4_Multicast

from someip_itf.codec import L4Proto, SdEntry, SdEntryType, SdMessage, SomeipResponse, build_sd, parse_someip
from someip_itf.tc8.asserts import assert_valid_response
from someip_itf.tc8.constants import (
    CYCLIC_OFFER_DELAY_MS,
    DUT_UNRELIABLE_PORT,
    EVENTGROUP_ID,
    INSTANCE_ID,
    MAJOR_VERSION,
    METHOD_ID,
    MULTICAST_EVENTGROUP_ID,
    SD_PORT,
    SERVICE_ID,
)
from someip_itf.tc8.message_builder import build_request
from someip_itf.tc8.sd import capture_sd_offers, open_multicast_socket, parse_sd_message, parse_sd_offers
from someip_itf.tc8.sd_sender import (
    build_subscribe_entry,
    capture_unicast_sd_entries,
    open_sender_socket,
    send_subscribe_eventgroup,
)
from someip_itf.tc8.timing import capture_sd_offers_with_timestamps

#: Time to wait for the next cyclic OfferService of the DUT after the test sends malformed data.
DUT_ALIVE_TIMEOUT: float = 5.0
#: The default TTL of send_subscribe_eventgroup. The Ack must have the same TTL as the Subscribe.
SUBSCRIBE_TTL: int = 3


def poll_sd_entries(
    sock: socket.socket,
    predicate,
    timeout_secs: float,
    resend=None,
    resend_interval_secs: float = 1.5,
) -> tuple[SomeipResponse, SdMessage, SdEntry]:
    """Read *sock* until an SD entry agrees with *predicate*. Return its message, SD header and entry.

    If *resend* is set, the function calls it at once and then every *resend_interval_secs* until an entry agrees.
    Raises `TimeoutError` if no entry agrees within *timeout_secs*.
    """
    if resend:
        resend()
    deadline = time.monotonic() + timeout_secs
    next_resend = time.monotonic() + resend_interval_secs if resend else None

    while time.monotonic() < deadline:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            break
        if next_resend is not None and time.monotonic() >= next_resend:
            resend()
            next_resend = time.monotonic() + resend_interval_secs
        sock.settimeout(min(remaining, 0.5 if resend else 1.0))
        try:
            data, _ = sock.recvfrom(65535)
        except socket.timeout:
            continue
        parsed = parse_sd_message(data)
        if parsed is None:
            continue
        someip_msg, sd_msg = parsed
        for entry in sd_msg.entries:
            if predicate(entry):
                return someip_msg, sd_msg, entry

    raise TimeoutError(f"No matching SD entry received within {timeout_secs:.1f}s")


def capture_multicast_entry(host_ip: str, predicate, timeout_secs: float) -> tuple[SomeipResponse, SdMessage, SdEntry]:
    """Capture the first SD entry on the SD multicast group that agrees with *predicate*.

    Raises `TimeoutError` if no entry arrives within *timeout_secs*.
    """
    sock = open_multicast_socket(host_ip)
    try:
        return poll_sd_entries(sock, predicate, timeout_secs)
    except TimeoutError:
        raise TimeoutError(
            f"No matching SD entry for service 0x{SERVICE_ID:04x} received within {timeout_secs:.1f}s on "
            f"{host_ip}:{SD_PORT}"
        ) from None
    finally:
        sock.close()


def capture_raw_sd_offer(
    host_ip: str,
    timeout_secs: float = 5.0,
) -> tuple[SomeipResponse, SdMessage, SdEntry]:
    """Capture the first OfferService for `SERVICE_ID` on the SD multicast group.

    Raises `TimeoutError` if no entry arrives within *timeout_secs*.
    """
    return capture_multicast_entry(
        host_ip, lambda e: e.sd_type == SdEntryType.OfferService and e.service_id == SERVICE_ID, timeout_secs
    )


def capture_subscribe_ack(
    dut_ip: str,
    tester_ip: str,
    eventgroup_id: int,
    timeout_secs: float = 5.0,
    ttl: int = SUBSCRIBE_TTL,
) -> SdEntry:
    """Subscribe to *eventgroup_id* with *ttl* and capture the SubscribeEventgroupAck entry.

    Raises `AssertionError` if no matching Ack arrives.
    """
    sock = open_sender_socket(tester_ip)
    try:
        sender_port: int = sock.getsockname()[1]

        def _send_sub() -> None:
            send_subscribe_eventgroup(
                sock,
                (dut_ip, SD_PORT),
                SERVICE_ID,
                INSTANCE_ID,
                eventgroup_id,
                MAJOR_VERSION,
                subscriber_ip=tester_ip,
                subscriber_port=sender_port,
                ttl=ttl,
            )

        _send_sub()
        entries = capture_unicast_sd_entries(
            sock,
            filter_types=(SdEntryType.SubscribeAck,),
            timeout_secs=timeout_secs,
            resend=_send_sub,
            max_results=1,
        )
        acks = [e for e in entries if e.eventgroup_id == eventgroup_id and e.ttl > 0]
        assert acks, (
            f"No SubscribeEventgroupAck received for eventgroup 0x{eventgroup_id:04x} within {timeout_secs:.1f}s"
        )
        return acks[0]
    finally:
        sock.close()


def capture_subscribe_ack_with_options(
    dut_ip: str,
    tester_ip: str,
    eventgroup_id: int,
    timeout_secs: float = 5.0,
    match_type: bool = True,
) -> tuple[SdMessage, SdEntry]:
    """Subscribe to *eventgroup_id* and return the SD header and entry of the SubscribeEventgroupAck.

    If *match_type* is false, the function returns the first Subscribe or SubscribeAck entry with TTL > 0
    for *eventgroup_id*.
    """
    accepted_types = (SdEntryType.SubscribeAck,) if match_type else (SdEntryType.Subscribe, SdEntryType.SubscribeAck)
    sock = open_sender_socket(tester_ip)
    try:
        sender_port: int = sock.getsockname()[1]

        def _send_sub() -> None:
            send_subscribe_eventgroup(
                sock,
                (dut_ip, SD_PORT),
                SERVICE_ID,
                INSTANCE_ID,
                eventgroup_id,
                MAJOR_VERSION,
                subscriber_ip=tester_ip,
                subscriber_port=sender_port,
            )

        try:
            _, sd_msg, ack = poll_sd_entries(
                sock,
                lambda e: e.sd_type in accepted_types and e.eventgroup_id == eventgroup_id and e.ttl > 0,
                timeout_secs,
                resend=_send_sub,
            )
        except TimeoutError:
            raise AssertionError(
                f"No SubscribeEventgroupAck with options received for eventgroup 0x{eventgroup_id:04x} within "
                f"{timeout_secs:.1f}s"
            ) from None
        return sd_msg, ack
    finally:
        sock.close()


def udp_endpoint_option(entry: SdEntry, label: str) -> SDOption_IP4_EndPoint:
    """Return the UDP `IPv4EndpointOption` of the OfferService entry, from one of the two option runs.

    The offered instance has a UDP port and a TCP port. Thus, the entry has two endpoint options.
    The first option is not always the UDP option.
    """
    all_opts = list(entry.options_1) + list(entry.options_2)
    ipv4_opts = [o for o in all_opts if isinstance(o, SDOption_IP4_EndPoint)]
    udp_opts = [o for o in ipv4_opts if o.l4_proto == L4Proto.UDP]
    assert udp_opts, f"{label}: No UDP IPv4EndpointOption found in OfferService entry"
    return udp_opts[0]


def multicast_option(ack: SdEntry, label: str) -> SDOption_IP4_Multicast:
    """Return the `IPv4MulticastOption` of the SubscribeAck, from one of the two option runs."""
    all_opts = list(ack.options_1) + list(ack.options_2)
    mcast_opts = [o for o in all_opts if isinstance(o, SDOption_IP4_Multicast)]
    assert mcast_opts, (
        f"{label}: No IPv4MulticastOption found in SubscribeAck for eventgroup 0x{MULTICAST_EVENTGROUP_ID:04x}"
    )
    return mcast_opts[0]


def encoded_subscribe_entry(subscriber_ip: str, ttl: int) -> bytes:
    """Return the 16 bytes of the Subscribe entry that the sender makes for `EVENTGROUP_ID` with *ttl*."""
    entry = build_subscribe_entry(
        SERVICE_ID, INSTANCE_ID, EVENTGROUP_ID, MAJOR_VERSION, subscriber_ip, SD_PORT, ttl=ttl
    )
    datagram = build_sd([entry], session_id=1)
    # The entries array starts after the 16-byte SOME/IP header, the SD Flags/Reserved word and its Length field.
    first_entry = 16 + 4 + 4
    return datagram[first_entry : first_entry + 16]


def collect_offers(
    host_ip: str,
    window_secs: float,
    stop_after_service_id: int | None = None,
) -> dict[int, SdEntry]:
    """Collect OfferService entries within *window_secs*. Keep the last entry for each service_id.

    If *stop_after_service_id* is set, return when an entry for it arrives. If not, wait for the full window.
    Skip the test if the multicast socket cannot be opened.
    """
    try:
        sock = open_multicast_socket(host_ip, port=SD_PORT)
    except OSError as exc:
        pytest.skip(f"Multicast socket setup failed on {host_ip}: {exc}")

    seen: dict[int, SdEntry] = {}
    deadline = time.monotonic() + window_secs
    try:
        while time.monotonic() < deadline:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                break
            sock.settimeout(min(remaining, 1.0))
            try:
                data, _ = sock.recvfrom(65535)
            except socket.timeout:
                continue
            for entry in parse_sd_offers(data):
                seen[entry.service_id] = entry
            if stop_after_service_id is not None and stop_after_service_id in seen:
                return seen
    finally:
        sock.close()

    return seen


def wait_for_main_phase_anchor_offer(tc8_host_ip: str) -> float:
    """Wait until the Repetition Phase ends, then return the monotonic timestamp of the next OfferService.

    The offer timing tests measure from this offer.
    """
    # Wait until the Repetition Phase ends (200+400+800 ms, repetitions_max=3, approximately 1.5 s).
    # The DUT is then in its cyclic Main Phase.
    time.sleep(3.5)

    timed = capture_sd_offers_with_timestamps(tc8_host_ip, count=1, timeout_secs=(CYCLIC_OFFER_DELAY_MS / 1000.0) + 2.0)
    service_offers = [(ts, e) for ts, e in timed if e.service_id == SERVICE_ID]
    assert service_offers, "no OfferService captured to start the timing measurement"
    return service_offers[-1][0]


def send_request_and_receive(
    tc8_dut_ip: str,
    request_bytes: bytes,
    timeout_secs: float = 3.0,
    *,
    with_addr: bool = False,
):
    """Send a SOME/IP request to the DUT and return the first response.

    With *with_addr*, also return the ``(ip, port)`` of the response.
    """
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        sock.bind(("", 0))
        sock.sendto(request_bytes, (tc8_dut_ip, DUT_UNRELIABLE_PORT))
        sock.settimeout(timeout_secs)
        data, addr = sock.recvfrom(65535)
        resp = parse_someip(data)
        return (resp, addr) if with_addr else resp
    finally:
        sock.close()


def send_request_expect_no_response(
    tc8_dut_ip: str,
    request_bytes: bytes,
    timeout_secs: float = 2.0,
) -> list:
    """Send a SOME/IP request and make sure that no response arrives."""
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    collected = []
    try:
        sock.bind(("", 0))
        sock.sendto(request_bytes, (tc8_dut_ip, DUT_UNRELIABLE_PORT))
        deadline = time.monotonic() + timeout_secs
        while time.monotonic() < deadline:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                break
            sock.settimeout(min(remaining, 0.5))
            try:
                data, _ = sock.recvfrom(65535)
                msg = parse_someip(data)
                collected.append(msg)
            except socket.timeout:
                continue
            except Exception:
                continue
    finally:
        sock.close()
    return collected


def assert_dut_alive(tc8_dut_ip: str, session_id: int) -> None:
    """Send a valid Echo request and make sure that the daemon still answers.

    Use it after a request that the DUT can ignore. It shows if the DUT ignored the request or stopped.
    """
    req = build_request(SERVICE_ID, METHOD_ID, client_id=0x00FF, session_id=session_id, interface_version=MAJOR_VERSION)
    resp = send_request_and_receive(tc8_dut_ip, req)
    assert_valid_response(resp, SERVICE_ID, METHOD_ID)


def require_dut_offer(host_ip: str, timeout: float, *, on_missing: str) -> None:
    """Return when the DUT sends a multicast OfferService. If not, skip or fail the test, as *on_missing* sets.

    Use ``"skip"`` for a precondition. No offer within *timeout* usually means that multicast does not work here.
    Use ``"fail"`` for a check after the test sends bad data.
    No cyclic offer means that the DUT or its SD stack stopped.
    """
    try:
        capture_sd_offers(host_ip, min_count=1, timeout_secs=timeout)
    except (TimeoutError, OSError) as exc:
        if on_missing == "skip":
            pytest.skip("DUT did not offer service within timeout — multicast may be unavailable")
        pytest.fail(f"DUT is not alive — no cyclic OfferService received within {timeout:.0f}s ({exc})")


def wait_for_dut_offer(tc8_host_ip: str, timeout: float = 5.0) -> None:
    """Skip the test if the DUT does not send a multicast OfferService within *timeout* seconds."""
    require_dut_offer(tc8_host_ip, timeout, on_missing="skip")


def verify_dut_alive(host_ip: str) -> None:
    """Fail the test if the next cyclic OfferService does not arrive within :data:`DUT_ALIVE_TIMEOUT` seconds.

    This time is more than two times the offer cycle of 2.0 s. No offer means that the DUT or its SD stack stopped.
    """
    require_dut_offer(host_ip, DUT_ALIVE_TIMEOUT, on_missing="fail")
