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

load("@rules_cc//cc:defs.bzl", "cc_library")
load("@score_baselibs//score/language/safecpp:toolchain_features.bzl", "COMPILER_WARNING_FEATURES")
load("//quality/unit_testing:unit_testing.bzl", "cc_unit_test")

cc_library(
    name = "api",
    hdrs = glob(["api_under_test/**/*.h"]),
    features = COMPILER_WARNING_FEATURES,
    strip_include_prefix = "api_under_test",
    deps = [
        "//score/mw/com",
        "@score_baselibs//score/memory/shared:types",
    ],
)

cc_unit_test(
    name = "generated_api_test",
    srcs = ["generated_api_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":api",
        "@score_baselibs//score/mw/log",
    ],
)
