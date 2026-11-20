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
"""Helpers that start and stop the SOME/IP stack of the target: the lg-someip routing daemon and the Tc8Interface app.

The daemon must start first, because it owns the local socket that the app registers through.
The target sends nothing on the network before the app offers. Thus, a check for readiness waits for the offer.

Only the suites that restart the stack during a live capture use these helpers directly.
The other suites use the stack that the ``someipd_dut`` fixture starts.
"""

from __future__ import annotations

import logging
import socket
import struct
import time
from typing import Optional

import pytest

from someip_itf.dut import TC8, launch_app, launch_daemon
from someip_itf.tc8.constants import SD_MULTICAST_ADDR, SD_PORT

logger = logging.getLogger(__name__)

#: SOME/IP-SD entry type for OfferService.
_SD_ENTRY_OFFER_SERVICE = 0x01
#: Service id every SOME/IP-SD message carries.
_SD_SERVICE_ID = 0xFFFF


class TargetStack:
    """The two processes that make up the target's SOME/IP stack.

    It has ``poll()``, because the tests check it the same way as a local subprocess.
    """

    def __init__(self, target, daemon_process, service_process) -> None:
        self._target = target
        self._daemon = daemon_process
        self._service = service_process

    def poll(self) -> Optional[int]:
        """Return ``None`` while the two processes run. Return ``0`` when one of them has stopped."""
        return None if (self._daemon.is_running() and self._service.is_running()) else 0

    def stop(self) -> None:
        """Stop the app, then the daemon. Log errors, but do not raise them."""
        for name, process in (("tc8_app", self._service), ("someip-daemon", self._daemon)):
            try:
                process.stop()
            except Exception:  # noqa: BLE001
                logger.warning("stopping %s raised during teardown", name, exc_info=True)


def start_stack(target, root: str = "") -> TargetStack:
    """Start the daemon and the Tc8Interface app on *target*, and return a handle on both.

    *root* is not used. It keeps the signature that the suites call.

    :raises RuntimeError: the daemon did not make its socket.
    """
    daemon_process = launch_daemon(target, TC8)
    return TargetStack(target, daemon_process, launch_app(target, TC8))


def start_offering_stack(target, root: str, host_ip: str) -> TargetStack:
    """Start the stack and return it when the target sends a multicast offer. Stop a stack that does not offer.

    The stack is ready at the first multicast offer, not when the processes run.
    The app registers with the daemon first. During this time, the two processes run but send nothing.
    *host_ip* is the address of the host on the target network. The multicast join uses it.

    :raises RuntimeError: as :func:`start_stack`.
    """
    stack = start_stack(target, root)
    if not wait_for_sd_readiness(host_ip):
        stack.stop()
        pytest.fail(
            "the target started but never multicast an OfferService; check that the service offered (its log is "
            f"streamed above) and that the host joined the group on {host_ip}."
        )
    return stack


def wait_for_sd_readiness(tester_ip: str, timeout_secs: float = 15.0) -> bool:
    """Return whether the target sends a multicast OfferService within *timeout_secs*.

    The offer is the only correct signal for readiness. The two processes can run before the offer.
    A test that sends before the offer gets no reply.

    :param tester_ip: The address of the host on the target network. The helper joins the group on this interface.
    """
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
    except AttributeError:
        pass
    sock.bind(("", SD_PORT))
    sock.setsockopt(
        socket.IPPROTO_IP,
        socket.IP_ADD_MEMBERSHIP,
        struct.pack("4s4s", socket.inet_aton(SD_MULTICAST_ADDR), socket.inet_aton(tester_ip)),
    )

    deadline = time.monotonic() + timeout_secs
    try:
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                return False
            sock.settimeout(min(remaining, 1.0))
            try:
                data, _ = sock.recvfrom(65535)
            except socket.timeout:
                continue
            if _carries_offer_service(data):
                return True
    finally:
        sock.close()


def _carries_offer_service(data: bytes) -> bool:
    """Return whether *data* is a SOME/IP-SD message with at least one OfferService entry.

    This function parses the bytes itself, so that the readiness check needs only the standard library.
    Layout: a 16-byte SOME/IP header, the SD flags and reserved bytes, the length of the entries array, and the entries.
    Each entry has 16 bytes.
    """
    header_size = 16
    entries_length_offset = header_size + 4
    entries_offset = entries_length_offset + 4
    entry_size = 16

    if len(data) < entries_offset or int.from_bytes(data[0:2], "big") != _SD_SERVICE_ID:
        return False

    entries_length = int.from_bytes(data[entries_length_offset:entries_offset], "big")
    end = min(entries_offset + entries_length, len(data))
    for position in range(entries_offset, end - entry_size + 1, entry_size):
        if data[position] == _SD_ENTRY_OFFER_SERVICE:
            return True
    return False
