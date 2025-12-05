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

cc_library(
    name = "unit_2",
    srcs = ["bar.cpp"],
    hdrs = ["bar.h"],
    visibility = ["//visibility:public"],
    deps = ["//third_party/traceability/doc/sample_library/unit_1"],
)

cc_test(
    name = "unit_2_test",
    srcs = ["bar_test.cpp"],
    visibility = ["//visibility:public"],
    deps = [
        ":unit_2",
        "@googletest//:gtest_main",
    ],
)
