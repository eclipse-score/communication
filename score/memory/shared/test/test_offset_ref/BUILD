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

load("//quality/unit_testing:unit_testing.bzl", "cc_unit_test")
load("//score/mw:common_features.bzl", "COMPILER_WARNING_FEATURES")

cc_unit_test(
    name = "offset_ref_test",
    srcs = ["offset_ref_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        "//score/memory/shared:offset_ref",
        "//score/memory/shared/fake:fake_memory_resources",
    ],
)

cc_unit_test(
    name = "offset_ref_precondition_violation_test",
    srcs = ["offset_ref_bounds_check_test.cpp"],
    features = COMPILER_WARNING_FEATURES + [
        # This test uses SCORE_LANGUAGE_FUTURECPP_EXPECT_CONTRACT_VIOLATED that relies on exceptions being thrown so we
        # disable aborts_upon_exception.
        "-aborts_upon_exception",
    ],
    deps = [
        "//score/memory/shared:offset_ref",
        "//score/memory/shared/fake:fake_memory_resources",
        "//score/memory/shared/test:bounds_check_memory_pool",
        "@score_baselibs//score/language/futurecpp:futurecpp_test_support",
    ],
)
