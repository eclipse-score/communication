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

"""Offer-TTL expiry after a SIGKILL, as the subscriber sees it.

The tests also cover a related gap. E_NOT_READY means "Service ID and Method ID are known.
Application not running." The SIGKILL in this file makes exactly this state.
After the kill, the DUT sends no StopOffer. For up to the 3 s offer TTL, the daemon keeps
the ServiceInfo entry and its this_client mapping. Thus ServiceCommunicator::HandleRequest
finds this_client != 0 and forwards the request through UDSAppsGateway::DeliverMessage.
Its server_->SendTo(...) call to the dead client_id does not check the result.
Thus the failure is not reported, and an external requester gets no reply instead of ERROR/E_NOT_READY.

The SIGKILL stops only the ``someip_dut`` process in the running container, not the container.
After the kill, the app process stays dead for the remaining life of the session-scoped container.
Thus each SIGKILL scenario has its OWN class. The module-level ``ets_dut`` override kills and
starts the DUT again for each class. Thus each class starts with a new DUT.

Offer TTL = 3 s. The routing daemon withdraws the service when the local socket of the killed app closes.
Thus the service stops to answer at the latest when the offer TTL ends, 4.5 s after the kill with jitter.

Test cases
----------
- A kill without StopOffer causes offer-TTL expiry in approximately 3 s.
- BUG (xfail): a REQUEST sent shortly after the kill (well in the offer-TTL window)
  must get ERROR/E_NOT_READY, but gets no reply.
- The same gap near the END of the offer-TTL window, not directly after the kill.
  This makes sure that the gap is not a short startup race.
"""

from __future__ import annotations

import asyncio
import time

import pytest
from attribute_plugin import add_test_properties
from someip_itf.dut import kill_dut
from someip_itf.ets.client import MessageType, ReturnCode
from someip_itf.ets.constants import ETS_SERVICE_ID, METHOD_ECHO_UINT8
from someip_itf.ets.fixtures import ets_dut  # noqa: F401 -- overrides the conftest fixture per class
from someip_itf.ets.raw import ets_frame, send_request_and_capture_reply

pytestmark = pytest.mark.asyncio(loop_scope="class")

# The offer TTL is 3 s. The tolerance of 1.5 s is for CI scheduling and reaper jitter.
_TTL_MAX = 4.5

# The raw-socket tests need their own reply port.
# This port is different from the ports of other test modules.
_RAW_TEST_PORT = 60024
_REQUEST_RESPONSE_TIMEOUT = 3.0


class TestEtsServiceAvailabilityKill:
    @add_test_properties(
        partially_verifies=["AS__PRS_SOMEIPSD_00356"],
        test_type="fault-injection",
        derivation_technique="error-guessing",
    )
    async def test_offer_ttl_expiry_on_kill(self, ets_client, ets_dut):  # noqa: F811 -- the per-class override
        """After a SIGKILL (no StopOffer), the service stops to answer in the 3 s offer TTL (+1.5 s).

        is_available() is a method-call probe. Thus the test does not see the offer TTL directly.
        """
        assert await ets_client.is_available(), "pre-condition: service must be offered"

        kill_dut(ets_dut)

        t_kill = time.monotonic()
        deadline = t_kill + _TTL_MAX + 0.5
        elapsed: float | None = None
        while time.monotonic() < deadline:
            if not await ets_client.is_available():
                elapsed = time.monotonic() - t_kill
                break
            await asyncio.sleep(0.1)

        assert elapsed is not None, (
            f"is_available() remained True for >{_TTL_MAX + 0.5:.1f} s after SIGKILL; "
            f"offer-TTL is 3 s, the daemon must expire the service in approximately 3.1 s"
        )
        assert elapsed <= _TTL_MAX, (
            f"the service stopped answering {elapsed:.2f} s after SIGKILL; expected at most {_TTL_MAX:.1f} s"
        )


class TestEtsServiceNotReadyShortlyAfterKill:
    """Separate class. The module-level ``ets_dut`` override gives the SIGKILL in this test
    a new service, not the service that ``test_offer_ttl_expiry_on_kill`` killed.
    """

    # E_NOT_READY means "Service ID and Method ID are known. Application not running."
    # A REQUEST well in the offer-TTL window after a SIGKILL is in exactly this state.
    # UDSAppsGateway::DeliverMessage sends to the dead client_id and does not check the result.
    # Thus the request is lost, and no ERROR/E_NOT_READY comes back.
    @add_test_properties(
        partially_verifies=["AS__PRS_SOMEIP_00191"],
        test_type="fault-injection",
        derivation_technique="error-guessing",
    )
    @pytest.mark.xfail(
        strict=True,
        reason=(
            "UDSAppsGateway::DeliverMessage sends to a dead client_id "
            "and does not check the result. A request in the offer-TTL "
            "window after a SIGKILL gets no reply instead of ERROR/E_NOT_READY"
        ),
    )
    async def test_request_shortly_after_kill_returns_e_not_ready(self, ets_client, ets_dut, host_ip):  # noqa: F811 -- the per-class override
        """A request sent 0.3 s after a SIGKILL, in the offer-TTL window, gets ERROR/E_NOT_READY."""
        assert await ets_client.is_available(), "pre-condition: service must be offered"

        kill_dut(ets_dut)
        await asyncio.sleep(0.3)  # well in the 3 s offer-TTL window

        frame = ets_frame(ETS_SERVICE_ID, METHOD_ECHO_UINT8, b"\x42")
        try:
            data, _ = await send_request_and_capture_reply(
                frame, host_ip, _REQUEST_RESPONSE_TIMEOUT, port=_RAW_TEST_PORT
            )
        except asyncio.TimeoutError:
            pytest.fail(
                "Expected ERROR/E_NOT_READY shortly after SIGKILL (service "
                "still known, application not running), but the server "
                "sent no reply."
            )

        assert data[14] == int(MessageType.ERROR)
        assert data[15] == int(ReturnCode.E_NOT_READY)


class TestEtsServiceNotReadyLateInTtlWindowAfterKill:
    """Separate class, for the same reason as TestEtsServiceNotReadyShortlyAfterKill."""

    # Same gap as `test_request_shortly_after_kill_returns_e_not_ready`, but near the END of the
    # offer-TTL window (approximately 2.7 s after the kill, still < 3 s).
    # This makes sure that the gap is not a short startup race.
    # The gap stays for the full "known but dead" window.
    @add_test_properties(
        partially_verifies=["AS__PRS_SOMEIP_00191"],
        test_type="fault-injection",
        derivation_technique="error-guessing",
    )
    @pytest.mark.xfail(
        strict=True,
        reason=(
            "Same UDSAppsGateway::DeliverMessage gap as "
            "test_request_shortly_after_kill_returns_e_not_ready, "
            "near the end of the offer-TTL window and not directly after "
            "the kill"
        ),
    )
    async def test_request_late_in_ttl_window_after_kill_returns_e_not_ready(self, ets_client, ets_dut, host_ip):  # noqa: F811 -- the per-class override
        """A request sent 2.7 s after a SIGKILL, late in the offer-TTL window, gets ERROR/E_NOT_READY."""
        assert await ets_client.is_available(), "pre-condition: service must be offered"

        kill_dut(ets_dut)
        await asyncio.sleep(2.7)  # still in the 3 s offer-TTL window

        frame = ets_frame(ETS_SERVICE_ID, METHOD_ECHO_UINT8, b"\x42")
        try:
            data, _ = await send_request_and_capture_reply(
                frame, host_ip, _REQUEST_RESPONSE_TIMEOUT, port=_RAW_TEST_PORT
            )
        except asyncio.TimeoutError:
            pytest.fail(
                "Expected ERROR/E_NOT_READY late in the offer-TTL window after SIGKILL, but the server sent no reply."
            )

        assert data[14] == int(MessageType.ERROR)
        assert data[15] == int(ReturnCode.E_NOT_READY)
