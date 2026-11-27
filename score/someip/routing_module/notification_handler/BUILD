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
    visibility = ["//score/someip/routing_module/notification_handler:__pkg__"],
    deps = ["@score_baselibs//score/language/futurecpp"],
)

cc_unit_test(
    name = "someip_message_header_parser_test",
    srcs = ["someip_message_header_parser_test.cpp"],
    deps = [":someip_message_header_parser"],
)

cc_library(
    name = "notification_handler",
    srcs = ["notification_handler.cpp"],
    hdrs = ["notification_handler.h"],
    visibility = ["//score/someip/routing_module:__subpackages__"],
    deps = [
        ":someip_message_header_parser",
        "//score/mw/com",
        "//score/someip/routing_control_protocol",
        "//score/someip/routing_module/lola_mapping",
        "//score/someip/routing_module/routing_manager:i_routing_command_handler",
        "//score/someip/routing_module/udp_transport:log",
        "//score/someip/routing_module/udp_transport:udp_packet_reader",
        "//score/someip/routing_module/udp_transport:udp_transport_api",
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/mw/log",
    ],
)

cc_unit_test(
    name = "notification_handler_test",
    srcs = ["notification_handler_test.cpp"],
    tags = ["exclusive"],
    deps = [
        ":notification_handler",
        "//score/mw/com",
        "//score/someip/routing_module/lola_mapping",
        "//score/someip/routing_module/udp_transport:udp_packet_reader",
        "//score/someip/routing_module/udp_transport:udp_transport_api",
        "@score_baselibs//score/filesystem",
        "@score_baselibs//score/mw/log:recorder_mock",
    ],
)

unit(
    name = "notification_handler_unit",
    design_name = "NotificationHandler",
    maturity = "development",
    scope = ["//score/someip/routing_module/notification_handler:__subpackages__"],
    tags = ["local"],  # LoLa report actions need host-unique PIDs.
    test_specs = [],
    tests = [
        ":notification_handler_test_linux",
        ":someip_message_header_parser_test_linux",
    ],
    unit_design = ["//score/someip/routing_module/notification_handler/dependability:notification_handler_unit_design"],
    visibility = [
        "//score/someip/dependability:__subpackages__",
        "//score/someip/routing_module:__pkg__",
    ],
    implementation = [
        ":notification_handler",
        ":someip_message_header_parser",
    ],
)
