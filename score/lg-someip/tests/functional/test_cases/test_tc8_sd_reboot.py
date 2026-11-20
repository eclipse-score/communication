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
"""TC8 SD reboot detection ("Reboot Flag").

The tests make sure that the DUT resets its SD state (the Reboot flag and the session_id counter)
at each cold start, not only at the first one.
The tests restart the service and check the flag in the outgoing traffic of the DUT.
The tests do not check how the DUT detects the reboot of a peer.
The stack restarts two times. The tests check the reboot flag and the session_id reset after each restart.
A daemon that clears its reboot state only one time passes a check of one restart, but fails on the second.
The same applies to a session counter that does not reset at each restart.

This suite needs its own stack lifecycle (start, drain, stop, restart, capture).
It must not share the class-scoped ``someipd_dut`` fixture of other TC8 suites.
A restart of a shared stack puts the other class into an unpredictable state.
"""

from typing import Generator, List, Tuple

import pytest
from attribute_plugin import add_test_properties
from someip_itf.codec import SdMessage, SomeipResponse
from someip_itf.tc8.dut_lifecycle import start_stack
from someip_itf.tc8.sd import collect_sd_messages_from_socket, open_multicast_socket

#: How long to wait for SD messages after each (re)start.
_SD_CAPTURE_TIMEOUT_SECS: float = 15.0

#: Number of messages to drain after the initial boot. Thus the SD state of the DUT is stable before the first
#: restart. The count is the initial offer plus some early repetitions.
_DRAIN_COUNT: int = 3


@pytest.fixture(scope="module")
def reboot_captures(tc8_network, dut_target, dut_root) -> Generator[List[Tuple[SomeipResponse, SdMessage]], None, None]:
    """Restart the stack twice, and return the first SD message captured after each restart.

    The fixture opens the multicast socket *before* the `start_stack` call of each restart.
    Thus the first message after the restart is not lost.
    """
    host_ip = tc8_network.gateway

    # Initial boot: let the SD state become stable after the first cold start, before the restarts.
    stack = start_stack(dut_target, dut_root)
    try:
        collect_sd_messages_from_socket(
            open_multicast_socket(host_ip), count=_DRAIN_COUNT, timeout_secs=_SD_CAPTURE_TIMEOUT_SECS
        )
    finally:
        stack.stop()

    captures: List[Tuple[SomeipResponse, SdMessage]] = []
    for _ in range(2):  # First restart, then a second restart.
        sock = open_multicast_socket(host_ip)
        try:
            stack = start_stack(dut_target, dut_root)
            try:
                messages = collect_sd_messages_from_socket(sock, count=1, timeout_secs=_SD_CAPTURE_TIMEOUT_SECS)
                captures.append(messages[0])
            finally:
                stack.stop()
        finally:
            sock.close()

    yield captures


class TestSDReboot:
    """The DUT resets the SD state (reboot flag and session_id) at each restart."""

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_reboot"],
        partially_verifies=["AS__PRS_SOMEIPSD_00255", "SOMEIPSRV_FORMAT_07"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    @pytest.mark.parametrize("restart_index", [0, 1], ids=["first_restart", "second_restart"])
    def test_tc8_sd_012_reboot_flag_set_after_restart(
        self,
        reboot_captures: List[Tuple[SomeipResponse, SdMessage]],
        restart_index: int,
    ) -> None:
        """The first SD message after a restart has the Reboot flag set."""
        assert reboot_captures, "No SD messages captured after restart"
        _, sd_hdr = reboot_captures[restart_index]

        assert sd_hdr.flag_reboot, (
            f"Reboot flag not set in the first SD message after restart #{restart_index + 1}. The DUT "
            "must reset its Reboot flag at each cold start."
        )

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_reboot"],
        partially_verifies=["AS__PRS_SOMEIPSD_00158", "SOMEIPSRV_FORMAT_07"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    @pytest.mark.parametrize("restart_index", [0, 1], ids=["first_restart", "second_restart"])
    def test_tc8_sd_012_session_id_resets_after_restart(
        self,
        reboot_captures: List[Tuple[SomeipResponse, SdMessage]],
        restart_index: int,
    ) -> None:
        """The SD session_id resets to 1 after a restart."""
        assert reboot_captures, "No SD messages captured after restart"
        outer, _ = reboot_captures[restart_index]

        assert outer.session_id == 1, (
            f"session_id after restart #{restart_index + 1} = {outer.session_id}; expected 1 "
            "(the session counter must reset at each reboot)"
        )
