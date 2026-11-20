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

"""SOME/IP-SD Subscribe Eventgroup option-run conformance.

An entry can reference options through two option runs (Index/Number of Options 1 and 2).
Both run layouts are equally valid ways to encode the same set of options.
Neither is a "primary" or a "fallback" layout.
The DUT must send a Subscribe Eventgroup Ack for the same options with each run layout.
"""

from __future__ import annotations

import pytest
from attribute_plugin import add_test_properties
from scapy.contrib.automotive.someip import SDOption_Config
from someip_itf.ets.client import (
    SD,
    SDENTRY_TYPE_EVTGRP_SUBSCRIBE,
    SOMEIP,
    MessageType,
    ReturnCode,
    SDEntry_EventGroup,
    SDOption_IP4_EndPoint,
)
from someip_itf.ets.constants import ETS_INSTANCE_ID, ETS_MAJOR_VERSION, ETS_SERVICE_ID, EVENTGROUP_UNICAST
from someip_itf.ets.raw import send_and_capture_subscribe_ack

pytestmark = pytest.mark.asyncio(loop_scope="class")

_NO_RESPONSE_TIMEOUT = 3.0


class TestSomeipSubscribeOptionRuns:
    @add_test_properties(
        partially_verifies=["SOMEIP_ETS_172", "SOMEIP_ETS_173"],
        test_type="requirements-based",
        derivation_technique="requirements-analysis",
    )
    async def test_sd_172_173_subscribe_ack_both_option_run_layouts(self, ets_dut, host_ip, tester_ip):  # noqa: ARG002
        """The DUT sends an Ack to a Subscribe with two options in each option-run layout.

        Variant 1 references one option through each run (``index_1=0``/``n_opt_1=1``, ``index_2=1``/
        ``n_opt_2=1``). Variant 2 references both options through the first run only (``index_1=0``/
        ``n_opt_1=2``, ``index_2=0``/``n_opt_2=0``). The test sends each variant as a separate unicast
        Subscribe. Each Subscribe must get a positive Ack.
        """
        ep = SDOption_IP4_EndPoint(addr=tester_ip, l4_proto=0x11, port=60093)
        extra = SDOption_Config(cfg_str=SDOption_Config.make_string({"unused": "1"}))

        variants = (
            (
                "split across both runs",
                SDEntry_EventGroup(
                    type=SDENTRY_TYPE_EVTGRP_SUBSCRIBE,
                    srv_id=ETS_SERVICE_ID,
                    inst_id=ETS_INSTANCE_ID,
                    major_ver=ETS_MAJOR_VERSION,
                    ttl=3,
                    eventgroup_id=EVENTGROUP_UNICAST,
                    index_1=0,
                    n_opt_1=1,
                    index_2=1,
                    n_opt_2=1,
                ),
            ),
            (
                "both in run 1",
                SDEntry_EventGroup(
                    type=SDENTRY_TYPE_EVTGRP_SUBSCRIBE,
                    srv_id=ETS_SERVICE_ID,
                    inst_id=ETS_INSTANCE_ID,
                    major_ver=ETS_MAJOR_VERSION,
                    ttl=3,
                    eventgroup_id=EVENTGROUP_UNICAST,
                    index_1=0,
                    n_opt_1=2,
                    index_2=0,
                    n_opt_2=0,
                ),
            ),
        )

        for session_id, (label, entry) in enumerate(variants, start=1):
            frame = bytes(
                SOMEIP(
                    srv_id=0xFFFF,
                    sub_id=0x8100,
                    client_id=0,
                    session_id=session_id,
                    proto_ver=0x01,
                    iface_ver=0x01,
                    msg_type=int(MessageType.NOTIFICATION),
                    retcode=int(ReturnCode.E_OK),
                )
                / SD(entry_array=[entry], option_array=[ep, extra])
            )
            ack = await send_and_capture_subscribe_ack(frame, host_ip, tester_ip)
            assert ack is not None, f"no SubscribeEventgroupAck received for {label}"
            assert ack.ttl > 0, f"expected a positive Ack for {label}; got ttl={ack.ttl}"
