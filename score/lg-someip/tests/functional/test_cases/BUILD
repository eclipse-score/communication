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

load("//quality/integration_testing:integration_testing.bzl", "integration_test")
load(":suites.bzl", "ETS_SUITES", "TC8_SUITES")

_COMMON_ARGS = [
    "-vvv",
    # One target per suite: its cases share one container and one stack.
    "--keep-target",
]

# The tester binds the SD port on the host and reaches the container over its docker bridge.
_COMMON_TAGS = [
    "exclusive",
    "requires-network",
]

[
    integration_test(
        name = SUITE_NAME,
        timeout = SUITE_TIMEOUT,
        srcs = [
            "conftest.py",
            SUITE_SRC,
        ],
        args = _COMMON_ARGS,
        filesystem = "//score/lg-someip/tests/functional:{}_filesystem".format(FAMILY),
        tags = _COMMON_TAGS + (["manual"] if SUITE_NAME == "ets_stress" else []),
        deps = ["//score/lg-someip/tests/functional/test_cases/helpers:someip_itf"],
    )
    for (FAMILY, SUITES) in [
        ("ets", ETS_SUITES),
        ("tc8", TC8_SUITES),
    ]
    for (SUITE_NAME, SUITE_SRC, SUITE_TIMEOUT) in SUITES
]

test_suite(
    name = "ets",
    tests = [SUITE_NAME for (SUITE_NAME, _, _) in ETS_SUITES if SUITE_NAME != "ets_stress"],
)

test_suite(
    name = "tc8",
    tests = [SUITE_NAME for (SUITE_NAME, _, _) in TC8_SUITES],
)
