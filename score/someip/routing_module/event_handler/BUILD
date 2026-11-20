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
    name = "someip_message_header_parser",
    srcs = [
        "someip_datagram_parser.cpp",
        "someip_message_header_parser.cpp",
    ],
    hdrs = [
        "someip_datagram_parser.h",
        "someip_message_header_parser.h",
    ],
    visibility = ["//score/someip/routing_module/event_handler:__pkg__"],
    deps = ["@score_baselibs//score/language/futurecpp"],
)

cc_unit_test(
    name = "someip_message_header_parser_test",
    srcs = ["someip_message_header_parser_test.cpp"],
    deps = [":someip_message_header_parser"],
)

unit(
    name = "event_handler",
    design_name = "EventHandler",
    maturity = "development",
    scope = ["//score/someip/routing_module/event_handler:__subpackages__"],
    test_specs = [],
    tests = [":someip_message_header_parser_test_linux"],
    unit_design = ["//score/someip/routing_module/event_handler/dependability:event_handler_unit_design"],
    visibility = [
        "//score/someip/dependability:__subpackages__",
        "//score/someip/routing_module:__pkg__",
    ],
    implementation = [":someip_message_header_parser"],
)
