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
"""TC8 multi-service identifier isolation tests.

A DUT that knows more than one service must offer its primary service correctly.
It must not send an unexpected service_id on the wire.
The reference ``--tc8-standalone`` DUT also offers only "service A" (0x1234/0x5678).
"Service B" (0x5678/0x0001) is only in its vsomeip JSON configuration, to show that the configuration parses.
This model has no equivalent for that.
The three test cases pass with a DUT that offers only service A. Thus the suite needs no second service instance.
It makes sure that the single-service offer does not change into an offer of a different service.
"""

from typing import Set

from attribute_plugin import add_test_properties
from someip_itf.tc8.asserts import assert_offer_has_ipv4_endpoint_option
from someip_itf.tc8.constants import DUT_UNRELIABLE_PORT
from someip_itf.tc8.dut_lifecycle import TargetStack
from someip_itf.tc8.probes import collect_offers

#: Service A: the one service this deployment actually offers.
_SERVICE_A_ID: int = 0x1234
_SERVICE_A_INSTANCE: int = 0x5678

#: Service B: this deployment and the reference --tc8-standalone DUT do not offer it.
#: Only the allow-list of service IDs uses it.
_SERVICE_B_ID: int = 0x5678
_SERVICE_B_INSTANCE: int = 0x0001

_DUT_READY_TIMEOUT_SECS: float = 10.0
_SD_OFFER_COLLECTION_WINDOW_SECS: float = 6.0


class TestMultiServiceInstanceRouting:
    """Service ID isolation on the wire."""

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__multi_service", "AS__PRS_SOMEIPSD_00355"],
        partially_verifies=["AS__PRS_SOMEIPSD_00356", "AS__SWS_CM_00203", "SOMEIPSRV_RPC_13"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_rpc_13_primary_service_offered(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
    ) -> None:
        """The DUT offers service A with a non-zero TTL (an active offer, not a StopOffer)."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        seen = collect_offers(tc8_host_ip, _DUT_READY_TIMEOUT_SECS, stop_after_service_id=_SERVICE_A_ID)
        offered = seen.get(_SERVICE_A_ID)
        assert offered is not None, (
            f"Service A (0x{_SERVICE_A_ID:04X}/0x{_SERVICE_A_INSTANCE:04X}) not offered within "
            f"{_DUT_READY_TIMEOUT_SECS}s"
        )

        assert offered.instance_id == _SERVICE_A_INSTANCE, (
            f"Unexpected instance_id in the OfferService of service A: 0x{offered.instance_id:04X}"
        )
        assert offered.ttl > 0, "OfferService TTL is 0 (StopOffer), the service is not active"

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__multi_service", "AS__PRS_SOMEIPSD_00306"],
        partially_verifies=[
            "AS__PRS_SOMEIPSD_00310",
            "AS__PRS_SOMEIPSD_00357",
            "AS__PRS_SOMEIPSD_00307",
            "SOMEIPSRV_RPC_14",
        ],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_rpc_14_service_a_advertises_configured_udp_port(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
        tc8_dut_ip: str,
    ) -> None:
        """The OfferService endpoint option of service A has the configured UDP port."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        seen = collect_offers(tc8_host_ip, _DUT_READY_TIMEOUT_SECS, stop_after_service_id=_SERVICE_A_ID)
        offered = seen.get(_SERVICE_A_ID)
        assert offered is not None, f"No OfferService for service A within {_DUT_READY_TIMEOUT_SECS}s"

        assert_offer_has_ipv4_endpoint_option(offered, expected_ip=tc8_dut_ip, expected_port=DUT_UNRELIABLE_PORT)

    @add_test_properties(
        fully_verifies=["comp_req__tc8_conformance__multi_service"],
        partially_verifies=["AS__PRS_SOMEIPSD_00356", "SOMEIPSRV_RPC_14"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    def test_rpc_14_no_unexpected_service_ids_in_offers(
        self,
        someipd_dut: TargetStack,
        tc8_host_ip: str,
    ) -> None:
        """Each received OfferService service_id is one of the two identifiers that this suite knows."""
        assert someipd_dut.poll() is None, "someipd DUT is not running"

        seen = collect_offers(tc8_host_ip, _SD_OFFER_COLLECTION_WINDOW_SECS)

        assert _SERVICE_A_ID in seen, (
            f"No OfferService for service A received in a {_SD_OFFER_COLLECTION_WINDOW_SECS}s window"
        )

        allowed_ids: Set[int] = {_SERVICE_A_ID, _SERVICE_B_ID}
        unexpected = set(seen.keys()) - allowed_ids
        assert not unexpected, (
            f"Unexpected service IDs in SD offers: {[hex(s) for s in unexpected]}. "
            f"Only {[hex(s) for s in allowed_ids]} are known."
        )
