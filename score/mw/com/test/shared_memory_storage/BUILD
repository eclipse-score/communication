# *******************************************************************************
# Copyright (c) 2026 Contributors to the Eclipse Foundation
#
# See the NOTICE file(s) distributed with this work for additional
# information regarding copyright ownership.
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

cc_library(
    name = "test_resources",
    srcs = [
        "test_resources.cpp",
        "test_resources.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//score/mw/com/impl/test:__subpackages__"],
    deps = [
        "//score/mw/com",
        "//score/mw/com/test/common_test_resources:bigdata_type",
        "@score_baselibs//score/os/utils/interprocess:interprocess_notification",
    ],
)
