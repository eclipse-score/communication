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

load("@score_baselibs//:bazel/unit_tests.bzl", "cc_gtest_unit_test", "cc_unit_test_suites_for_host_and_qnx")
load("//score/mw:common_features.bzl", "COMPILER_WARNING_FEATURES")

cc_library(
    name = "i_message_passing_service",
    srcs = [
        "i_message_passing_service.cpp",
    ],
    hdrs = [
        "i_message_passing_service.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = ["//score/mw/com/impl/bindings/lola:__pkg__"],
    deps = [
        "//score/mw/com/impl:scoped_event_receive_handler",
        "//score/mw/com/impl/bindings/lola:element_fq_id",
        "//score/mw/com/impl/configuration:quality_type",
    ],
)

cc_library(
    name = "node_identifier_copier",
    srcs = [
        "node_identifier_copier.cpp",
    ],
    hdrs = [
        "node_identifier_copier.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    deps = [
        "//score/mw/com/impl/bindings/lola:element_fq_id",
        "@score_baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "thread_abstraction",
    srcs = [
        "thread_abstraction.cpp",
    ],
    hdrs = [
        "thread_abstraction.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
)

cc_library(
    name = "messaging",
    srcs = [
        "message_passing_client_cache.cpp",
        "message_passing_service.cpp",
        "message_passing_service_instance.cpp",
    ],
    hdrs = [
        "message_passing_client_cache.h",
        "message_passing_service.h",
        "message_passing_service_instance.h",
        "thread_abstraction.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = ["//score/mw/com/impl/bindings/lola:__pkg__"],
    deps = [
        ":i_message_passing_service",
        "@communication//score/message_passing",
        "@score_baselibs//score/concurrency:thread_pool",
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/mw/log",
        "@score_baselibs//score/os:errno_logging",
    ],
)

cc_library(
    name = "message_passing_service_mock",
    testonly = True,
    srcs = [],
    hdrs = ["message_passing_service_mock.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//score/mw/com/impl:__subpackages__"],
    deps = [
        ":i_message_passing_service",
        "@googletest//:gtest",
    ],
)

cc_library(
    name = "thread_abstraction_mock",
    testonly = True,
    srcs = ["thread_abstraction_mock.cpp"],
    hdrs = ["thread_abstraction_mock.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//score/mw/com/impl:__subpackages__"],
    deps = [
        ":thread_abstraction",
        "@googletest//:gtest",
    ],
)

cc_test(
    name = "thread_abstraction_test",
    # TODO: Investigate why this unit tests takes more than 1 minute under memcheck (Ticket-141169)
    timeout = "moderate",
    srcs = [
        "thread_abstraction_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES,
    tags = ["unit"],
    visibility = [
        "//score/mw/com/impl/bindings/lola/messaging:__pkg__",
    ],
    deps = [
        ":thread_abstraction",
        ":thread_abstraction_mock",
        "@googletest//:gtest_main",
    ],
)

cc_gtest_unit_test(
    name = "node_identifier_copier_test",
    srcs = ["node_identifier_copier_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com/impl:__subpackages__",
    ],
    deps = [":node_identifier_copier"],
)

cc_unit_test_suites_for_host_and_qnx(
    name = "unit_test_suite",
    cc_unit_tests = [
        ":thread_abstraction_test",
        ":node_identifier_copier_test",
    ],
    test_suites_from_sub_packages = [
        "//score/mw/com/impl/bindings/lola/messaging/messages:unit_test_suite",
    ],
    visibility = ["//score/mw/com/impl/bindings/lola:__pkg__"],
)
