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

load("@rules_cc//cc:defs.bzl", "cc_library", "cc_shared_library")
load("@score_tooling//bazel/rules/rules_score:rules_score.bzl", "unit")
load("//quality/unit_testing:unit_testing.bzl", "cc_unit_test")

cc_library(
    name = "routing_log_adapter",
    srcs = ["routing_log_adapter.cpp"],
    deps = [
        "//score/someip/routing_module/udp_transport:log",
        "@score_baselibs//score/mw/log",
    ],
    alwayslink = True,
)

cc_library(
    name = "routing_bridge",
    srcs = ["routing_bridge.cpp"],
    hdrs = ["routing_bridge.h"],
    deps = [
        "//score/message_passing",
        "//score/someip/routing_module/notification_handler",
        "//score/someip/routing_module/routing_manager",
        "//score/someip/routing_module/routing_manager:i_routing_command_handler",
        "//score/someip/routing_module/udp_transport:udp_packet_reader",
        "//score/someip/routing_module/udp_transport:udp_transport_api",
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/os:errno",
    ],
)

# The someip_routing_* functions called by the platform transport.
cc_library(
    name = "routing_bridge_entry",
    srcs = ["routing_bridge_entry.cpp"],
    visibility = ["//score/someip/routing_module:__subpackages__"],
    deps = [
        ":routing_bridge",
        ":routing_log_adapter",
        "//score/message_passing",
        "//score/mw/com",
        "//score/someip/routing_module/udp_transport:udp_transport_api",
        "@score_baselibs//score/filesystem",
        "@score_baselibs//score/mw/log",
    ],
)

cc_shared_library(
    name = "mods_someip_routing",
    shared_lib_name = "mods-someip-routing.so",
    target_compatible_with = ["@platforms//os:qnx"],
    user_link_flags = ["-lfsnotify"],
    visibility = [
        "//score/someip/routing_module:__subpackages__",
        "//score/someip/test:__pkg__",
    ],
    deps = [
        ":routing_bridge_entry",
        "//score/someip/routing_module/udp_transport:platform_transport",
        "@score_logging//score/mw/log/backend:slog",
    ],
)

cc_unit_test(
    name = "routing_bridge_test",
    srcs = ["routing_bridge_test.cpp"],
    tags = ["exclusive"],
    deps = [
        ":routing_bridge",
        ":routing_bridge_entry",
        ":routing_log_adapter",
        "//score/message_passing:mock",
        "//score/mw/com",
        "//score/someip/routing_module/lola_mapping",
        "//score/someip/routing_module/udp_transport:udp_transport_api",
        "@score_baselibs//score/filesystem",
        "@score_baselibs//score/mw/log:recorder_mock",
    ],
)

cc_unit_test(
    name = "linux_bridge_test",
    srcs = ["linux_bridge_test.cpp"],
    tags = ["exclusive"],
    target_compatible_with = ["@platforms//os:linux"],
    deps = [
        ":routing_bridge",
        ":routing_log_adapter",
        "//score/message_passing:mock",
        "//score/mw/com",
        "//score/someip/routing_module/lola_mapping",
        "//score/someip/routing_module/udp_transport:linux_transport",
        "@score_baselibs//score/filesystem",
    ],
)

unit(
    name = "routing_bridge_unit",
    design_name = "RoutingBridge",
    maturity = "development",
    scope = ["//score/someip/routing_module/routing_bridge:__subpackages__"],
    test_specs = [],
    tests = [
        ":linux_bridge_test_linux",
        ":routing_bridge_test_linux",
    ],
    unit_design = ["//score/someip/routing_module/routing_bridge/dependability:routing_bridge_unit_design"],
    visibility = [
        "//score/someip/dependability:__subpackages__",
        "//score/someip/routing_module:__pkg__",
    ],
    implementation = [
        ":routing_bridge",
        ":routing_bridge_entry",
        ":routing_log_adapter",
    ],
)
