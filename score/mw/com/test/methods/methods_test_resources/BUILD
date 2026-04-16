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
    name = "proxy_container",
    srcs = ["proxy_container.cpp"],
    hdrs = ["proxy_container.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com/test/methods:__subpackages__",
    ],
    deps = [
        "//score/mw/com",
    ],
)

cc_library(
    name = "method_provider",
    srcs = ["method_provider.cpp"],
    hdrs = ["method_provider.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com/test/methods:__subpackages__",
    ],
    deps = [
        "//score/mw/com",
        "//score/mw/com/test/common_test_resources:assert_handler",
        "//score/mw/com/test/common_test_resources:sctf_test_runner",
        "@score_baselibs//score/mw/log",
        "@score_baselibs//score/result",
    ],
)

cc_library(
    name = "method_consumer",
    srcs = ["method_consumer.cpp"],
    hdrs = ["method_consumer.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com/test/methods:__subpackages__",
    ],
    deps = [
        "//score/mw/com",
        "//score/mw/com:runtime",
        "//score/mw/com/test/common_test_resources:assert_handler",
        "//score/mw/com/test/common_test_resources:sctf_test_runner",
        "//score/mw/com/test/methods/methods_test_resources:proxy_container",
        "@score_baselibs//score/mw/log",
    ],
)

cc_library(
    name = "process_synchronizer",
    srcs = ["process_synchronizer.cpp"],
    hdrs = ["process_synchronizer.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com/test/methods:__subpackages__",
    ],
    deps = [
        "//score/mw/com/test/common_test_resources:shared_memory_object_creator",
        "//score/mw/com/test/common_test_resources:shared_memory_object_guard",
        "@score_baselibs//score/mw/log",
        "@score_baselibs//score/os/utils/interprocess:interprocess_notification",
        "@score_baselibs//score/result",
    ],
)
