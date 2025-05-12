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
load("//score/mw:common_features.bzl", "COMPILER_WARNING_FEATURES")

cc_library(
    name = "messaging",
    srcs = [
        "handler_base.cpp",
        "i_message_passing_control.cpp",
        "i_message_passing_service.cpp",
        "message_passing_control.cpp",
        "message_passing_facade.cpp",
        "notify_event_handler.cpp",
        "thread_abstraction.cpp",
    ],
    hdrs = [
        "handler_base.h",
        "i_message_passing_control.h",
        "i_message_passing_service.h",
        "message_passing_control.h",
        "message_passing_facade.h",
        "notify_event_handler.h",
        "thread_abstraction.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = ["//score/mw/com/impl/bindings/lola:__pkg__"],
    deps = [
        "@score-baselibs//score/os:errno_logging",
        "@score-baselibs//score/os:unistd",
        "//score/mw/com/impl:scoped_event_receive_handler",
        "//score/mw/com/impl/bindings/lola:element_fq_id",
        "//score/mw/com/impl/bindings/lola/messaging/messages",
        "//score/mw/com/impl/configuration",
        "//score/mw/com/message_passing",
        "@score-baselibs//score/mw/log",
        "@score-baselibs//score/language/futurecpp",
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
        "//score/mw/com/impl/bindings/lola:__pkg__",
        # "@ddad//score/mw/com/impl/bindings/lola/messaging:__pkg__",
    ],
    deps = [
        ":messaging",
        ":mock",
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
        "//score/mw/com/impl/bindings/lola:__pkg__",
        # "@ddad//score/mw/com/impl/bindings/lola/messaging:__pkg__",
    ],
    deps = [
        ":messaging",
        ":mock",
        "@score-baselibs//score/language/safecpp/coverage_termination_handler",
        "//score/mw/com/message_passing",
        "//score/mw/com/message_passing:mock",
        "@score-baselibs//score/mw/log",
        "@googletest//:gtest_main",
    ],
)

test_suite(
    name = "unit_test_suite",
    tests = [
        ":message_passing_unit_test",
        ":other_unit_test",
        "//score/mw/com/impl/bindings/lola/messaging/messages:unit_test_suite",
    ],
    visibility = ["//score/mw/com/impl/bindings/lola:__pkg__"],
)
