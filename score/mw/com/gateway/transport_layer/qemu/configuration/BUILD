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
load("@score_baselibs//score/language/safecpp:toolchain_features.bzl", "COMPILER_WARNING_FEATURES")
load("//quality/unit_testing:unit_testing.bzl", "cc_unit_test")

package(
    default_visibility = ["//score/mw/com/gateway/transport_layer/qemu:__subpackages__"],
)

exports_files(["example/mw_com_gateway_qemu_transport_config.json"])

cc_library(
    name = "qemu_transport_config_parser",
    srcs = ["qemu_transport_config_parser.cpp"],
    hdrs = [
        "qemu_transport_config_parser.h",
        "qemu_transport_configuration.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = [
        "@score_baselibs//score/mw/log",
    ],
    visibility = ["//score/mw/com/gateway/transport_layer:__subpackages__"],
    deps = [
        "//score/mw/com/gateway/transport_layer/qemu/ivshmem:ivshmem_bar_discovery",
        "@score_baselibs//score/json",
        "@score_baselibs//score/language/futurecpp",
    ],
)

cc_unit_test(
    name = "qemu_transport_config_parser_test",
    srcs = ["qemu_transport_config_parser_test.cpp"],
    deps = [
        ":qemu_transport_config_parser",
        "@googletest//:gtest",
        "@score_baselibs//score/json",
    ],
)
