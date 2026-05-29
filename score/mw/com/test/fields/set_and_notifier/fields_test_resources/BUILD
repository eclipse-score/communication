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

cc_library(
    name = "test_datatype",
    srcs = ["test_datatype.cpp"],
    hdrs = ["test_datatype.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com/test/fields/set_and_notifier:__subpackages__",
    ],
    deps = [
        "//score/mw/com",
    ],
)

cc_library(
    name = "field_consumer",
    srcs = ["field_consumer.cpp"],
    hdrs = ["field_consumer.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com/test/fields/set_and_notifier:__subpackages__",
    ],
    deps = [
        ":test_datatype",
        "//score/mw/com",
    ],
)

cc_library(
    name = "field_provider",
    srcs = ["field_provider.cpp"],
    hdrs = ["field_provider.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com/test/fields/set_and_notifier:__subpackages__",
    ],
    deps = [
        ":test_datatype",
        "//score/mw/com",
    ],
)
