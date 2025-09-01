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
load("@score-baselibs//:bazel/unit_tests.bzl", "cc_gtest_unit_test", "cc_unit_test_suites_for_host_and_qnx")
load("//score/mw:common_features.bzl", "COMPILER_WARNING_FEATURES")

cc_library(
    name = "messaging",
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = ["//score/mw/com/impl/bindings/lola:__pkg__"],
    deps = [
        ":i_message_passing_control",
        ":i_message_passing_service",
        ":i_notify_event_handler",
        ":message_passing_control",
        ":message_passing_facade",
        ":notify_event_handler",
        ":thread_abstraction",
    ],
)

cc_library(
    name = "i_message_passing_control",
    srcs = [
        "i_message_passing_control.cpp",
    ],
    hdrs = [
        "i_message_passing_control.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = ["//score/mw/com/impl/bindings/lola:__pkg__"],
    deps = [
        "//score/mw/com/impl/configuration",
        "//score/mw/com/message_passing",
    ],
)

cc_library(
    name = "message_passing_control",
    srcs = [
        "message_passing_control.cpp",
    ],
    hdrs = [
        "message_passing_control.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = ["//score/mw/com/impl/bindings/lola:__pkg__"],
    deps = [
        ":i_message_passing_control",
        "@score-baselibs//score/concurrency:thread_pool",
    ],
)

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
        "//score/mw/com/impl/configuration",
    ],
)

cc_library(
    name = "message_passing_facade",
    srcs = [
        "message_passing_facade.cpp",
    ],
    hdrs = [
        "message_passing_facade.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = ["//score/mw/com/impl/bindings/lola:__pkg__"],
    deps = [
        ":i_message_passing_control",
        ":i_message_passing_service",
        ":i_notify_event_handler",
        ":thread_abstraction",
        "@score-baselibs//score/concurrency:thread_pool",
        "@score-baselibs//score/os:errno_logging",
        "//score/mw/com/impl:scoped_event_receive_handler",
        "//score/mw/com/impl/bindings/lola:element_fq_id",
        "//score/mw/com/impl/configuration",
        "//score/mw/com/message_passing",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "i_notify_event_handler",
    srcs = [
        "i_notify_event_handler.cpp",
    ],
    hdrs = [
        "i_notify_event_handler.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = ["//score/mw/com/impl/bindings/lola/messaging:__pkg__"],
    deps = [
        ":i_message_passing_service",
        "//score/mw/com/impl:scoped_event_receive_handler",
        "//score/mw/com/impl/bindings/lola:element_fq_id",
        "//score/mw/com/impl/bindings/lola/messaging/messages",
        "//score/mw/com/impl/configuration",
        "//score/mw/com/message_passing",
    ],
)

cc_library(
    name = "notify_event_handler",
    srcs = [
        "notify_event_handler.cpp",
    ],
    hdrs = [
        "notify_event_handler.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = [
        ":node_identifier_copier",
    ],
    tags = ["FFI"],
    deps = [
        ":i_message_passing_control",
        ":i_message_passing_service",
        ":i_notify_event_handler",
        "@score-baselibs//score/os:errno_logging",
        "@score-baselibs//score/os:unistd",
        "@score-baselibs//score/mw/log",
        "@score-baselibs//score/language/futurecpp",
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
        "@score-baselibs//score/language/futurecpp",
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
    deps = [
    ],
)

cc_library(
    name = "mock",
    testonly = True,
    srcs = ["thread_abstraction_mock.cpp"],
    hdrs = [
        "message_passing_control_mock.h",
        "message_passing_service_mock.h",
        "thread_abstraction_mock.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//score/mw/com/impl:__subpackages__"],
    deps = [
        ":messaging",
        "@googletest//:gtest",
    ],
)

cc_library(
    name = "notify_event_handler_mock",
    testonly = True,
    srcs = ["notify_event_handler_mock.cpp"],
    hdrs = ["notify_event_handler_mock.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//score/mw/com/impl/bindings/lola/messaging:__subpackages__"],
    deps = [
        ":i_notify_event_handler",
        "@googletest//:gtest",
    ],
)

cc_library(
    name = "notify_event_handler_facade",
    testonly = True,
    srcs = ["notify_event_handler_mock_facade.cpp"],
    hdrs = ["notify_event_handler_mock_facade.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//score/mw/com/impl/bindings/lola/messaging:__subpackages__"],
    deps = [
        ":i_notify_event_handler",
        ":notify_event_handler_mock",
    ],
)

cc_test(
    name = "message_passing_unit_test",
    # TODO: Investigate why this unit tests takes more than 1 minute under memcheck (Ticket-141169)
    timeout = "moderate",
    srcs = [
        "message_passing_control_test.cpp",
        "message_passing_facade_test.cpp",
        "thread_abstraction_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    tags = ["unit"],
    visibility = [
        "//score/mw/com/impl/bindings/lola/messaging:__pkg__",
    ],
    deps = [
        ":messaging",
        ":mock",
        ":notify_event_handler_facade",
        ":notify_event_handler_mock",
        "@score-baselibs//score/language/safecpp/coverage_termination_handler",
        "@score-baselibs//score/os/mocklib:unistd_mock",
        "//score/mw/com/message_passing",
        "//score/mw/com/message_passing:mock",
        "@score-baselibs//score/mw/log",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "other_unit_test",
    # TODO: Investigate why this unit tests takes more than 1 minute under memcheck (Ticket-141169)
    timeout = "moderate",
    srcs = [
        "notify_event_handler_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    tags = ["unit"],
    visibility = [
        # "//score/mw/com/impl/bindings/lola:__pkg__",
        "//score/mw/com/impl/bindings/lola/messaging:__pkg__",
    ],
    deps = [
        ":mock",
        ":notify_event_handler",
        "@score-baselibs//score/language/safecpp/coverage_termination_handler",
        "//score/mw/com/message_passing",
        "//score/mw/com/message_passing:mock",
        "@score-baselibs//score/mw/log",
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
        ":message_passing_unit_test",
        ":node_identifier_copier_test",
        ":other_unit_test",
    ],
    test_suites_from_sub_packages = [
        "//score/mw/com/impl/bindings/lola/messaging/messages:unit_test_suite",
    ],
    visibility = ["//score/mw/com/impl/bindings/lola:__pkg__"],
)
