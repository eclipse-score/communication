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
    name = "unit_1",
    srcs = ["foo.cpp"],
    hdrs = ["foo.h"],
    visibility = ["//visibility:public"],
)

cc_test(
    name = "unit_1_test",
    srcs = ["foo_test.cpp"],
    visibility = ["//visibility:public"],
    deps = [
        ":unit_1",
        "@googletest//:gtest_main",
    ],
)
