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
load(":iosock_module.bzl", "iosock_copts", "iosock_sdp_inputs")

_QNX_ONLY = select({
    "@platforms//os:qnx": [],
    "//conditions:default": ["@platforms//:incompatible"],
})

config_setting(
    name = "qnx_x86_64",
    constraint_values = [
        "@platforms//cpu:x86_64",
        "@platforms//os:qnx",
    ],
)

config_setting(
    name = "qnx_aarch64",
    constraint_values = [
        "@platforms//cpu:aarch64",
        "@platforms//os:qnx",
    ],
)

cc_library(
    name = "iosock_transport_api",
    hdrs = ["iosock_transport.h"],
    visibility = ["//score/someip/routing_module:__subpackages__"],
)

cc_library(
    name = "log",
    srcs = ["log.c"],
    hdrs = ["log.h"],
    local_defines = ["_QNX_SOURCE"],
    target_compatible_with = _QNX_ONLY,
)

cc_library(
    name = "wakeup_signal",
    srcs = ["wakeup_signal.c"],
    hdrs = ["wakeup_signal.h"],
    local_defines = ["_QNX_SOURCE"],
    target_compatible_with = _QNX_ONLY,
    deps = [":iosock_transport_api"],
)

cc_library(
    name = "iosock_module",
    srcs = ["iosock_module.c"],
    copts = iosock_copts(":qnx_x86_64", ":qnx_aarch64"),
    data = iosock_sdp_inputs(":qnx_x86_64", ":qnx_aarch64"),
    target_compatible_with = _QNX_ONLY,
    visibility = ["//score/someip/routing_module:__subpackages__"],
    deps = [
        ":iosock_transport_api",
        ":log",
        ":wakeup_signal",
    ],
    alwayslink = True,
)

cc_library(
    name = "iosock_packet_reader",
    srcs = ["iosock_packet_reader.cpp"],
    hdrs = ["iosock_packet_reader.h"],
    visibility = ["//score/someip/routing_module:__subpackages__"],
    deps = [
        ":iosock_transport_api",
        "@score_baselibs//score/language/futurecpp",
    ],
)

cc_unit_test(
    name = "iosock_packet_reader_test",
    srcs = ["iosock_packet_reader_test.cpp"],
    deps = [":iosock_packet_reader"],
)

unit(
    name = "iosock_transport",
    design_name = "IoSockTransport",
    maturity = "development",
    scope = ["//score/someip/routing_module/iosock_transport:__subpackages__"],
    test_specs = [],
    tests = [":iosock_packet_reader_test_linux"],
    unit_design = ["//score/someip/routing_module/iosock_transport/dependability:iosock_transport_unit_design"],
    visibility = [
        "//score/someip/dependability:__subpackages__",
        "//score/someip/routing_module:__pkg__",
    ],
    implementation = [":iosock_packet_reader"],
)
