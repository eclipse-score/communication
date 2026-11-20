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
load("@score_tooling//bazel/rules/rules_score:rules_score.bzl", "unit")
load("//quality/unit_testing:unit_testing.bzl", "cc_unit_test")

cc_library(
    name = "i_routing_command_handler",
    hdrs = ["i_routing_command_handler.h"],
    visibility = ["//score/someip/routing_module:__subpackages__"],
    deps = ["//score/someip/routing_control_protocol"],
)

cc_library(
    name = "routing_command_handler_mock",
    testonly = True,
    hdrs = ["routing_command_handler_mock.h"],
    visibility = ["//score/someip/routing_module:__subpackages__"],
    deps = [
        ":i_routing_command_handler",
        "@googletest//:gtest",
    ],
)

cc_library(
    name = "routing_manager",
    srcs = ["routing_manager.cpp"],
    hdrs = ["routing_manager.h"],
    visibility = ["//score/someip/routing_module:__subpackages__"],
    deps = [
        ":i_routing_command_handler",
        "//score/message_passing",
        "//score/someip/routing_control_protocol",
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/mw/log",
        "@score_baselibs//score/os:errno",
    ],
)

cc_unit_test(
    name = "routing_manager_test",
    srcs = ["routing_manager_test.cpp"],
    visibility = ["//score/someip/routing_module:__subpackages__"],
    deps = [
        ":routing_command_handler_mock",
        ":routing_manager",
        "//score/message_passing:mock",
    ],
)

cc_unit_test(
    name = "routing_manager_message_passing_test",
    srcs = ["routing_manager_message_passing_test.cpp"],
    visibility = ["//score/someip/routing_module:__subpackages__"],
    deps = [
        ":routing_command_handler_mock",
        ":routing_manager",
        "//score/message_passing",
    ],
)

unit(
    name = "routing_manager_unit",
    design_name = "RoutingManager",
    maturity = "development",
    scope = [
        "//score/someip/routing_module/routing_manager:__subpackages__",
    ],
    target_compatible_with = ["@platforms//os:linux"],
    test_specs = [],
    tests = [
        ":routing_manager_message_passing_test_linux",
        ":routing_manager_test_linux",
    ],
    unit_design = ["//score/someip/routing_module/routing_manager/dependability:routing_manager_unit_design"],
    visibility = ["//score/someip/routing_module:__pkg__"],
    implementation = [":routing_manager"],
)
