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

load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")
load("@score_tooling//bazel/rules/rules_score:rules_score.bzl", "unit")
load("//quality/unit_testing:unit_testing.bzl", "cc_unit_test")
load(":qnx/module.bzl", "qnx_copts", "qnx_sdp_inputs")

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
    visibility = ["//visibility:private"],
)

config_setting(
    name = "qnx_aarch64",
    constraint_values = [
        "@platforms//cpu:aarch64",
        "@platforms//os:qnx",
    ],
    visibility = ["//visibility:private"],
)

cc_library(
    name = "udp_transport_api",
    hdrs = ["udp_transport.h"],
    visibility = ["//score/someip/routing_module:__subpackages__"],
)

cc_library(
    name = "log",
    srcs = ["log.c"],
    hdrs = ["log.h"],
    local_defines = ["_QNX_SOURCE"],
    visibility = ["//score/someip/routing_module:__subpackages__"],
)

cc_library(
    name = "qnx_wakeup_signal",
    srcs = ["qnx/wakeup_signal.c"],
    hdrs = ["qnx/wakeup_signal.h"],
    local_defines = ["_QNX_SOURCE"],
    target_compatible_with = _QNX_ONLY,
    deps = [":udp_transport_api"],
)

cc_library(
    name = "qnx_transport",
    srcs = ["qnx/transport.c"],
    hdrs = ["qnx/transport.h"],
    copts = qnx_copts(":qnx_x86_64", ":qnx_aarch64"),
    data = qnx_sdp_inputs(":qnx_x86_64", ":qnx_aarch64"),
    target_compatible_with = _QNX_ONLY,
    visibility = ["//score/someip/routing_module:__subpackages__"],
    deps = [
        ":log",
        ":qnx_wakeup_signal",
        ":udp_transport_api",
    ],
)

cc_library(
    name = "qnx_module",
    srcs = ["qnx/module.c"],
    copts = qnx_copts(":qnx_x86_64", ":qnx_aarch64"),
    data = qnx_sdp_inputs(":qnx_x86_64", ":qnx_aarch64"),
    target_compatible_with = _QNX_ONLY,
    deps = [
        ":log",
        ":qnx_transport",
        ":qnx_wakeup_signal",
        ":udp_transport_api",
    ],
    alwayslink = True,
)

cc_library(
    name = "udp_packet_reader",
    srcs = ["udp_packet_reader.cpp"],
    hdrs = ["udp_packet_reader.h"],
    visibility = ["//score/someip/routing_module:__subpackages__"],
    deps = [
        ":udp_transport_api",
        "@score_baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "linux_transport",
    srcs = ["linux/transport.c"],
    hdrs = ["linux/transport.h"],
    target_compatible_with = ["@platforms//os:linux"],
    visibility = ["//score/someip/routing_module:__subpackages__"],
    deps = [
        ":log",
        ":udp_transport_api",
    ],
)

cc_library(
    name = "linux_module",
    srcs = ["linux/module.c"],
    target_compatible_with = ["@platforms//os:linux"],
    deps = [
        ":linux_transport",
        ":log",
        ":udp_transport_api",
    ],
    alwayslink = True,
)

alias(
    name = "platform_transport",
    actual = select({
        "@platforms//os:linux": ":linux_module",
        "@platforms//os:qnx": ":qnx_module",
    }),
    visibility = ["//score/someip/routing_module:__subpackages__"],
)

cc_binary(
    name = "linux_routing_module",
    target_compatible_with = ["@platforms//os:linux"],
    visibility = ["//score/someip:__subpackages__"],
    deps = [
        ":platform_transport",
        "//score/someip/routing_module/routing_bridge:routing_bridge_entry",
    ],
)

cc_unit_test(
    name = "linux_transport_test",
    srcs = ["linux/transport_test.cpp"],
    linkopts = [
        "-Wl,--wrap=getsockopt",
        "-Wl,--wrap=poll",
        "-Wl,--wrap=recv",
        "-Wl,--wrap=recvfrom",
    ],
    linkstatic = True,
    target_compatible_with = ["@platforms//os:linux"],
    deps = [
        ":linux_transport",
        ":log",
        ":udp_transport_api",
    ],
)

cc_unit_test(
    name = "udp_packet_reader_test",
    srcs = ["udp_packet_reader_test.cpp"],
    deps = [":udp_packet_reader"],
)

unit(
    name = "udp_transport",
    design_name = "UdpTransport",
    maturity = "development",
    scope = ["//score/someip/routing_module/udp_transport:__subpackages__"],
    test_specs = [],
    tests = select({
        "@platforms//os:linux": [
            ":linux_transport_test_linux",
            ":udp_packet_reader_test_linux",
        ],
        "//conditions:default": [],
    }),
    unit_design = ["//score/someip/routing_module/udp_transport/dependability:udp_transport_unit_design"],
    visibility = [
        "//score/someip/dependability:__subpackages__",
        "//score/someip/routing_module:__pkg__",
    ],
    implementation = [":udp_packet_reader"] + select({
        "@platforms//os:linux": [
            ":linux_module",
            ":linux_transport",
        ],
        "@platforms//os:qnx": [
            ":qnx_module",
            ":qnx_transport",
            ":qnx_wakeup_signal",
        ],
        "//conditions:default": [],
    }),
)
