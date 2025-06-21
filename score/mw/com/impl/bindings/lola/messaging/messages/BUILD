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
load("@score-baselibs//bazel:unit_tests.bzl", "cc_gtest_unit_test", "cc_unit_test_suites_for_host_and_qnx")
load("//score/mw:common_features.bzl", "COMPILER_WARNING_FEATURES")

cc_library(
    name = "messages",
    srcs = [
        "message_common.cpp",
        "message_element_fq_id.cpp",
        "message_outdated_nodeid.cpp",
    ],
    hdrs = [
        "message_common.h",
        "message_element_fq_id.h",
        "message_outdated_nodeid.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = ["//score/mw/com/impl/bindings/lola/messaging:__pkg__"],
    deps = [
        "@score-baselibs//score/os:unistd",
        "//score/mw/com/impl/bindings/lola:element_fq_id",
        "//score/mw/com/message_passing",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_gtest_unit_test(
    name = "unit_test",
    srcs = [
        "message_common_test.cpp",
        "message_element_fq_id_test.cpp",
        "message_outdated_nodeid_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com/impl/bindings/lola/messaging/messages:__pkg__",
    ],
    deps = [
        ":messages",
        "@score-baselibs//score/mw/log",
    ],
)

cc_unit_test_suites_for_host_and_qnx(
    name = "unit_test_suite",
    cc_unit_tests = [
        ":unit_test",
    ],
    visibility = ["//score/mw/com/impl/bindings/lola/messaging:__pkg__"],
)
