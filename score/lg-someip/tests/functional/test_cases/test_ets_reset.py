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

"""Reset semantics.

The test sends resetInterface during an event burst.
"""

from __future__ import annotations

import asyncio

import pytest
from attribute_plugin import add_test_properties
from someip_itf.ets.constants import EVENT_UINT8, METHOD_RESET_INTERFACE, METHOD_TRIGGER_EVENT_UINT8, trigger

pytestmark = pytest.mark.asyncio(loop_scope="class")


class TestResetSemantics:
    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_146"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_reset_during_event_burst_no_duplicate_counters(self, ets_client, ets_eventgroup):
        samples: list[int] = []

        def _on_event(event_id: int, payload: bytes) -> None:
            if event_id == EVENT_UINT8:
                samples.append(payload[0])

        ets_client.register_callback(_on_event)
        ets_client.subscribe_eventgroup(ets_eventgroup, 30)

        try:
            # Burst: start=0 s, duration=8 s, period=1 s.
            # The 8 s burst continues after the reset. The wall time stays below 12 s.
            # One debounceTime tick is 100 ms. Thus debounceTime=10 gives 1 s/event,
            # and duration=80 gives an 8 s burst.
            await trigger(ets_client, METHOD_TRIGGER_EVENT_UINT8, bytes([0, 80, 10]))

            await asyncio.sleep(3.5)
            pre_reset_len = len(samples)
            assert pre_reset_len >= 2, f"expected ≥2 events before reset, got {pre_reset_len}"

            await trigger(ets_client, METHOD_RESET_INTERFACE, b"")

            # Wait until the burst completes, plus a margin.
            await asyncio.sleep(6.0)
        finally:
            ets_client.unsubscribe_eventgroup(ets_eventgroup)
            ets_client.register_callback(None)

        post_reset_count = len(samples) - pre_reset_len
        assert post_reset_count >= 2, f"expected ≥2 events after reset, got {post_reset_count}"

        # Main contract: no duplicate counter values before and after the reset.
        assert len(set(samples)) == len(samples), f"duplicate counter values before and after the reset: {samples}"

        # NOTE: "burst stops" or "counter goes back to 0" are stricter contracts.
        # The current server does neither. resetInterface does not change the local counter of the worker.
        # Thus the burst continues with monotonic values.
        # A check for a burst stop fails until the server changes.
        # Thus the test checks only the "no duplicates" contract.
