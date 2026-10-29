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

load("@bazel_skylib//rules:common_settings.bzl", "bool_flag")
load("@rules_cc//cc:defs.bzl", "cc_library")

# Build options. They mirror the ENABLE_* options of the CMake build and are
# all OFF by default, e.g. `--//score/lg-someip:enable_someip_tp=True`.
# ENABLE_TLS and ENABLE_DLT are not available in the Bazel build yet.
bool_flag(
    name = "enable_someip_tp",
    build_setting_default = False,
)

bool_flag(
    name = "enable_someip_packet_filtering",
    build_setting_default = False,
)

bool_flag(
    name = "enable_someip_delivery_statistics",
    build_setting_default = False,
)

bool_flag(
    name = "enable_someip_ipc",
    build_setting_default = False,
)

config_setting(
    name = "someip_tp_enabled",
    flag_values = {":enable_someip_tp": "True"},
    visibility = ["//score/lg-someip:__subpackages__"],
)

config_setting(
    name = "someip_packet_filtering_enabled",
    flag_values = {":enable_someip_packet_filtering": "True"},
    visibility = ["//score/lg-someip:__subpackages__"],
)

config_setting(
    name = "someip_delivery_statistics_enabled",
    flag_values = {":enable_someip_delivery_statistics": "True"},
    visibility = ["//score/lg-someip:__subpackages__"],
)

config_setting(
    name = "someip_ipc_enabled",
    flag_values = {":enable_someip_ipc": "True"},
    visibility = ["//score/lg-someip:__subpackages__"],
)

# Public lg-someip API headers (include/lgsomeip).
cc_library(
    name = "api_headers",
    hdrs = glob(["include/lgsomeip/*.h"]),
    includes = ["include"],
    visibility = ["//score/lg-someip:__subpackages__"],
)

# Header-only parameter serialization helpers (src/serialization).
cc_library(
    name = "serialization",
    hdrs = glob(["src/serialization/**/*.h"]),
    includes = ["src"],
    visibility = ["//score/lg-someip:__subpackages__"],
)

# Shared library with the lg-someip client API.
alias(
    name = "lgsomeip",
    actual = "//score/lg-someip/src/someip:lgsomeip",
    visibility = ["//score:__subpackages__"],
)

alias(
    name = "someip-daemon",
    actual = "//score/lg-someip/daemon:someip-daemon",
    visibility = ["//score:__subpackages__"],
)

alias(
    name = "vsomeip-if",
    actual = "//score/lg-someip/third_party/vsomeip_interface:vsomeip-if",
    visibility = ["//score:__subpackages__"],
)

test_suite(
    name = "tests",
    tests = ["//score/lg-someip/tests:all_tests"],
)
