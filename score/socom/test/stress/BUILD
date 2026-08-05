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

load("@rules_cc//cc:cc_test.bzl", "cc_test")

cc_test(
    name = "socom_stress_test",
    size = "small",
    srcs = glob(include = ["*.cpp"]),
    data = ["tsan.supp"],
    env = {
        "TSAN_OPTIONS": "suppressions=$(location tsan.supp)",
    },
    deps = [
        "//score/socom/test/framework:socom_test_framework",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
    ],
)
