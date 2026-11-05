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

load("@rules_pkg//pkg:mappings.bzl", "pkg_files")
load("//score/mw:common_features.bzl", "COMPILER_WARNING_FEATURES")
load("//score/mw/com/test:pkg_application.bzl", "pkg_application")

exports_files(
    [
        "configs/logging.json",
        "configs/mw_com_config_sender.json",
        "configs/mw_com_gateway_config_sender.json",
    ],
    visibility = ["//ecu/xpad/xpad-shared/config/ipnext/isoc/os:__subpackages__"],
)

cc_binary(
    name = "gateway",
    srcs = ["gateway.cpp"],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    visibility = [
        "//ecu/xpad/xpad-shared/config/ipnext/isoc/os:__subpackages__",
    ],
    deps = [
        "//score/mw/com:runtime",
        "//score/mw/com/gateway/gateway_application",
        "//score/mw/com/gateway/gateway_application/configuration:gateway_config_parser",
        "//score/mw/com/test/common_test_resources:command_line_parser",
        "//score/mw/com/test/common_test_resources:stop_token_sig_term_handler",
    ],
)

pkg_files(
    name = "etc_files",
    srcs = [
        "configs/logging.json",
        "configs/mw_com_config_receiver.json",
        "configs/mw_com_config_sender.json",
        "configs/mw_com_gateway_config_receiver.json",
        "configs/mw_com_gateway_config_sender.json",
        "configs/mw_com_gateway_transport_config_receiver.json",
        "configs/mw_com_gateway_transport_config_sender.json",
    ],
    strip_prefix = "configs",
)

pkg_application(
    name = "gateway-pkg",
    app_name = "gateway_app",
    bin = [":gateway"],
    bin_name = "gateway_app",
    etc_deps = [":etc_files"],
    visibility = [
        "//score/mw/com/test/hv_gateway_test:__subpackages__",
    ],
)
