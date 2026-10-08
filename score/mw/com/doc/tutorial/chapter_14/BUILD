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
load("@rules_pkg//pkg:tar.bzl", "pkg_tar")
load("@score_baselibs//score/language/safecpp:toolchain_features.bzl", "COMPILER_WARNING_FEATURES")
load("//score/mw/com/test:pkg_application.bzl", "pkg_application")

cc_library(
    name = "gateway_application_runner",
    srcs = ["gateway_application_runner.cpp"],
    hdrs = ["gateway_application_runner.h"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        "//score/mw/com/gateway/gateway_application",
        "//score/mw/com/gateway/gateway_application/configuration:gateway_config_parser",
        "@score_baselibs//score/mw/log",
    ],
)

cc_binary(
    name = "gateway_application_bin",
    srcs = ["gateway_application_main.cpp"],
    data = [
        ":logging.json",
    ],
    env = {"MW_LOG_CONFIG_FILE": "$(location logging.json)"},
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":gateway_application_runner",
        "//score/mw/com:runtime",
        "//score/mw/com:runtime_configuration",
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/mw/log",
    ],
)

# The same binary is packaged twice, once per gateway role. Each package carries the config files of its role under
# /opt/<app_name>/etc, which is also where the "config-path" in its mw_com_gateway_config_*.json points to.
pkg_application(
    name = "source_gateway-pkg",
    app_name = "SourceGateway",
    bin = [":gateway_application_bin"],
    etc = [
        "etc/mw_com_config_source_gateway.json",
        "etc/mw_com_gateway_config_source_gateway.json",
        "etc/mw_com_gateway_sample_transport_config_source_gateway.json",
        "logging.json",
    ],
    visibility = [
        "//score/mw/com/doc:__subpackages__",
    ],
)

pkg_tar(
    name = "source_gateway-tar",
    srcs = [":source_gateway-pkg"],
    visibility = [
        "//score/mw/com/doc:__subpackages__",
    ],
)

pkg_application(
    name = "destination_gateway-pkg",
    app_name = "DestinationGateway",
    bin = [":gateway_application_bin"],
    etc = [
        "etc/mw_com_config_destination_gateway.json",
        "etc/mw_com_gateway_config_destination_gateway.json",
        "etc/mw_com_gateway_sample_transport_config_destination_gateway.json",
        "logging.json",
    ],
    visibility = [
        "//score/mw/com/doc:__subpackages__",
    ],
)

pkg_tar(
    name = "destination_gateway-tar",
    srcs = [":destination_gateway-pkg"],
    visibility = [
        "//score/mw/com/doc:__subpackages__",
    ],
)

filegroup(
    name = "chapter_14",
    srcs = [
        ":destination_gateway-tar",
        ":source_gateway-tar",
    ],
    visibility = ["//score/mw/com/doc:__subpackages__"],
)

filegroup(
    name = "doc_files",
    srcs = glob([
        "**/*.rst",
        "**/*.cpp",
        "**/*.h",
        "**/*.json",
    ]),
    visibility = ["//score/mw/com/doc/tutorial:__pkg__"],
)
