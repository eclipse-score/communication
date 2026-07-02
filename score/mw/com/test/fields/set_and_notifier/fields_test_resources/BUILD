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
    name = "field_types",
    srcs = [
        "initial_only_field.cpp",
        "set_enabled_field.cpp",
        "test_constants.cpp",
    ],
    hdrs = [
        "initial_only_field.h",
        "set_enabled_field.h",
        "test_constants.h",
    ],
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
        ":field_types",
        "//score/mw/com",
        "//score/mw/com/test/common_test_resources:fail_test",
        "//score/mw/com/test/common_test_resources:process_synchronizer",
        "//score/mw/com/test/common_test_resources:proxy_container",
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
        ":field_types",
        "//score/mw/com",
        "//score/mw/com/test/common_test_resources:fail_test",
        "//score/mw/com/test/common_test_resources:process_synchronizer",
        "//score/mw/com/test/common_test_resources:skeleton_container",
    ],
)
