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

load("//quality/unit_testing:unit_testing.bzl", "cc_unit_test")
load("//score/mw:common_features.bzl", "COMPILER_WARNING_FEATURES")

cc_unit_test(
    name = "e2e_profile_stub_test",
    srcs = ["e2e_profile_stub_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    deps = ["//score/mw/com/impl/e2e:e2e_profile_stub"],
)
