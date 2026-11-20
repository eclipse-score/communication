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
"""TC8 SD phase timing tests.

The tests make sure that someipd goes through the Repetition Phase (short offers, the interval doubles)
before the Main Phase (long cyclic offers at a fixed interval).
The capture must start before the first offer of the DUT.
Thus the tests cannot use the shared, class-scoped ``someipd_dut`` fixture.
When a test with that fixture starts, the Repetition Phase is already complete.
This suite starts its own stack, and opens the capture socket first.
"""

from itertools import takewhile
from typing import Generator, List, Tuple

import pytest
from attribute_plugin import add_test_properties
from someip_itf.codec import SdEntry
from someip_itf.tc8.constants import CYCLIC_OFFER_DELAY_MS, SERVICE_ID
from someip_itf.tc8.dut_lifecycle import start_stack
from someip_itf.tc8.sd import open_multicast_socket
from someip_itf.tc8.timing import collect_sd_offers_from_socket

_LGSOMEIP_6_XFAIL_REASON = (
    "[lg-someip #6] the first Main Phase offer comes later than one CYCLIC_OFFER_DELAY after the last repetition "
    "(PRS_SOMEIPSD_00411)"
)

#: SD server configuration values (SOMEIP-SD-SERVER-SERVICE-INSTANCE-CONFIG).
_REPETITIONS_MAX: int = 3
_REPETITIONS_BASE_DELAY_MS: float = 200.0

#: Each Repetition Phase gap must be double the previous gap. Real container and network jitter is
#: much larger than a tight 2x check allows. Thus a gap counts as "doubled" when its ratio to the
#: previous gap is in this band.
_DOUBLING_RATIO_MIN: float = 1.3
_DOUBLING_RATIO_MAX: float = 3.0

#: A Main Phase gap can differ from cyclic_offer_delay by this fraction (±20%).
_MAIN_PHASE_GAP_TOLERANCE: float = 0.2

#: The capture timeout includes DUT startup, the Repetition Phase and one cyclic gap: initial delay (<=100 ms) +
#: Repetition Phase gaps (0.2s, 0.4s, 0.8s, a doubling series with cyclic_offer_delay as upper limit) + one 2 s
#: cyclic gap, with a large margin for container startup.
_PHASE_CAPTURE_TIMEOUT_SECS: float = 20.0

#: Captured offers: initial + up to 3 repetitions + 1 cyclic, as set by _REPETITIONS_MAX.
_OFFERS_TO_CAPTURE: int = 5


@pytest.fixture(scope="module")
def phase_capture(tc8_network, dut_target, dut_root) -> Generator[List[Tuple[float, SdEntry]], None, None]:
    """Start a new stack and capture SD offers with timestamps, from before the first offer.

    The fixture opens the multicast socket before `start_stack`. Thus the first offer, which starts the
    Repetition Phase, is not lost. The fixture is not class-scoped like `someipd_dut`. This suite needs the
    startup of the stack, not a stack that already runs.
    """
    host_ip = tc8_network.gateway
    sock = open_multicast_socket(host_ip)
    stack = start_stack(dut_target, dut_root)
    try:
        offers = collect_sd_offers_from_socket(sock, count=_OFFERS_TO_CAPTURE, timeout_secs=_PHASE_CAPTURE_TIMEOUT_SECS)
    except TimeoutError:
        offers = []  # The tests have their own assertions for an empty capture.
    finally:
        sock.close()
        stack.stop()

    yield offers


class TestSDPhasesTiming:
    """Repetition Phase interval and count."""

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_phases_timing"],
        partially_verifies=["AS__PRS_SOMEIPSD_00405", "AS__PRS_SOMEIPSD_00404", "SOMEIPSRV_SD_BEHAVIOR_01"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_tc8_sd_009_repetition_phase_intervals(self, phase_capture: List[Tuple[float, SdEntry]]) -> None:
        """The first gap between offers is the base delay of the Repetition Phase.

        The check is stricter than "shorter than the cyclic period". The first Repetition Phase gap is
        ``INITIAL-REPETITIONS-BASE-DELAY`` (200 ms here). A capture that starts on the first offer of the DUT
        sees that gap directly. A capture that starts one offer late sees the *second* gap, which is
        already doubled to approximately 400 ms. Thus the test accepts both.
        """
        service_offers = [(ts, e) for ts, e in phase_capture if e.service_id == SERVICE_ID]
        assert len(service_offers) >= 2, "Not enough OfferService entries captured for timing analysis"

        gap_ms = (service_offers[1][0] - service_offers[0][0]) * 1000.0
        lo = _REPETITIONS_BASE_DELAY_MS / _DOUBLING_RATIO_MAX
        hi = (_REPETITIONS_BASE_DELAY_MS * 2) * _DOUBLING_RATIO_MAX

        assert lo <= gap_ms <= hi, (
            f"First inter-offer gap {gap_ms:.0f} ms is not a Repetition Phase base-delay gap; expected "
            f"~{_REPETITIONS_BASE_DELAY_MS:.0f} ms (or ~{_REPETITIONS_BASE_DELAY_MS * 2:.0f} ms if the very first "
            f"offer was missed), within [{lo:.0f}, {hi:.0f}] ms"
        )

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__sd_phases_timing"],
        partially_verifies=[
            "AS__PRS_SOMEIPSD_00406",
            "AS__PRS_SOMEIPSD_00407",
            "AS__PRS_SOMEIPSD_00410",
            "AS__PRS_SOMEIPSD_00411",
            "AS__PRS_SOMEIPSD_00412",
            "SOMEIPSRV_SD_BEHAVIOR_01",
        ],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    # Not strict: the gap lands within a few milliseconds of the tolerance edge, so some runs pass.
    @pytest.mark.xfail(strict=False, reason=_LGSOMEIP_6_XFAIL_REASON)
    def test_tc8_sd_010_repetition_count_before_main_phase(self, phase_capture: List[Tuple[float, SdEntry]]) -> None:
        """Some short offer gaps, each approximately double the previous gap, then gaps of the cyclic offer delay.

        The capture can miss the first offer. Thus the test also accepts one short gap fewer than repetitions_max.
        There must never be more short gaps than repetitions_max.
        """
        service_offers = [(ts, e) for ts, e in phase_capture if e.service_id == SERVICE_ID]
        assert len(service_offers) >= 2, "too few offers captured to measure a gap"

        cyclic_half_ms = CYCLIC_OFFER_DELAY_MS * 0.5
        gaps_ms = [(service_offers[i + 1][0] - service_offers[i][0]) * 1000.0 for i in range(len(service_offers) - 1)]
        # Short gaps belong to the Repetition Phase, and they all come first.
        short_gaps = list(takewhile(lambda gap: gap < cyclic_half_ms, gaps_ms))
        main_gaps = gaps_ms[len(short_gaps) :]

        min_short_gaps = _REPETITIONS_MAX - 1
        assert min_short_gaps <= len(short_gaps) <= _REPETITIONS_MAX, (
            f"{len(short_gaps)} short gap(s); expected {min_short_gaps} to {_REPETITIONS_MAX}"
        )

        for i in range(len(short_gaps) - 1):
            ratio = short_gaps[i + 1] / short_gaps[i]
            assert _DOUBLING_RATIO_MIN <= ratio <= _DOUBLING_RATIO_MAX, (
                f"short gap {i + 2} ({short_gaps[i + 1]:.0f} ms) is {ratio:.2f}x gap {i + 1} "
                f"({short_gaps[i]:.0f} ms); expected about 2x, within [{_DOUBLING_RATIO_MIN}, {_DOUBLING_RATIO_MAX}]"
            )

        assert main_gaps, "no long gap after the short gaps, thus the Main Phase did not start"
        allowed_ms = CYCLIC_OFFER_DELAY_MS * _MAIN_PHASE_GAP_TOLERANCE
        for i, gap in enumerate(main_gaps):
            assert abs(gap - CYCLIC_OFFER_DELAY_MS) <= allowed_ms, (
                f"long gap {i + 1} is {gap:.0f} ms; expected {CYCLIC_OFFER_DELAY_MS:.0f} ms +/- {allowed_ms:.0f} ms"
            )
