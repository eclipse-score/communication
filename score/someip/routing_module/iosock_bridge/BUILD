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
    name = "iosock_bridge",
    srcs = ["iosock_bridge.cpp"],
    hdrs = ["iosock_bridge.h"],
    deps = [
        "//score/message_passing",
        "//score/someip/routing_module/iosock_transport:iosock_packet_reader",
        "//score/someip/routing_module/iosock_transport:iosock_transport_api",
        "//score/someip/routing_module/notification_handler",
        "//score/someip/routing_module/routing_manager",
        "//score/someip/routing_module/routing_manager:i_routing_command_handler",
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/os:errno",
    ],
)

# The someip_routing_* functions called by the io-sock module.
cc_library(
    name = "iosock_bridge_entry",
    srcs = ["iosock_bridge_entry.cpp"],
    visibility = ["//score/someip/routing_module:__subpackages__"],
    deps = [
        ":iosock_bridge",
        "//score/message_passing",
        "//score/mw/com",
        "//score/someip/routing_module/iosock_transport:iosock_transport_api",
        "@score_baselibs//score/filesystem",
        "@score_baselibs//score/mw/log",
    ],
)

cc_unit_test(
    name = "iosock_bridge_test",
    srcs = ["iosock_bridge_test.cpp"],
    deps = [
        ":iosock_bridge",
        "//score/message_passing:mock",
        "//score/mw/com",
        "//score/someip/routing_module/iosock_transport:iosock_transport_api",
        "//score/someip/routing_module/lola_mapping",
        "@score_baselibs//score/filesystem",
    ],
)

unit(
    name = "iosock_bridge_unit",
    design_name = "IoSockBridge",
    maturity = "development",
    scope = ["//score/someip/routing_module/iosock_bridge:__subpackages__"],
    test_specs = [],
    tests = [":iosock_bridge_test_linux"],
    unit_design = ["//score/someip/routing_module/iosock_bridge/dependability:iosock_bridge_unit_design"],
    visibility = [
        "//score/someip/dependability:__subpackages__",
        "//score/someip/routing_module:__pkg__",
    ],
    implementation = [
        ":iosock_bridge",
        ":iosock_bridge_entry",
    ],
)
