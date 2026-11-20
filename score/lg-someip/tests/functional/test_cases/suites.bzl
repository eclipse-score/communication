# *******************************************************************************
# Copyright (c) 2025 Contributors to the Eclipse Foundation
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

"""The SOME/IP suite inventory: (suite name, source, bazel timeout)."""

#: ETS suites; the ETS test app is their DUT. test_ets_stress runs on request only.
ETS_SUITES = [
    ("ets_" + NAME, "test_ets_" + NAME + ".py", "long")
    for NAME in [
        "array",
        "availability",
        "availability_kill",
        "bit_integrity",
        "client_proxy",
        "echo",
        "ecu_mode",
        "endianness",
        "error_handling",
        "events",
        "fields",
        "negative_scenarios",
        "reliable",
        "reset",
        "routing",
        "sd_subscribe",
        "sd_timing",
        "smoke",
        "strings",
        "tp",
        "union",
    ]
] + [("ets_stress", "test_ets_stress.py", "eternal")]

#: TC8 conformance suites; the Tc8Interface test app is their DUT.
TC8_SUITES = [
    ("tc8_service_discovery", "test_tc8_service_discovery.py", "eternal"),
    ("tc8_someip_message_format", "test_tc8_someip_message_format.py", "eternal"),
    ("tc8_event_notification", "test_tc8_event_notification.py", "long"),
    ("tc8_field_conformance", "test_tc8_field_conformance.py", "long"),
    ("tc8_multi_service", "test_tc8_multi_service.py", "long"),
    ("tc8_sd_format_compliance", "test_tc8_sd_format_compliance.py", "eternal"),
    ("tc8_sd_robustness", "test_tc8_sd_robustness.py", "eternal"),
    ("tc8_sd_phases_timing", "test_tc8_sd_phases_timing.py", "long"),
    ("tc8_sd_reboot", "test_tc8_sd_reboot.py", "long"),
]
