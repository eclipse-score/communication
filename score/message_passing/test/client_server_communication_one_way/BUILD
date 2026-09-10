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
load("@score_baselibs//score/language/safecpp:toolchain_features.bzl", "COMPILER_WARNING_FEATURES")
load("//bazel/tools:json_schema_validator.bzl", "validate_json_schema_test")
load("//score/mw/com/test:pkg_application.bzl", "pkg_application")

cc_library(
    name = "common_resources",
    srcs = ["common_resources.cpp"],
    hdrs = ["common_resources.h"],
    features = COMPILER_WARNING_FEATURES,
    deps = ["//score/message_passing"],
)

cc_binary(
    name = "client",
    srcs = ["client.cpp"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":common_resources",
        "//score/message_passing",
        "@score_baselibs//score/string_manipulation/arguments",
    ],
)

cc_binary(
    name = "server",
    srcs = ["server.cpp"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":common_resources",
        "//score/message_passing",
        "@score_baselibs//score/string_manipulation/arguments",
    ],
)

pkg_application(
    name = "client-pkg",
    app_name = "ClientApp",
    bin = [":client"],
    visibility = [
        "//score/message_passing/test/client_server_communication_one_way:__subpackages__",
    ],
)

pkg_application(
    name = "server-pkg",
    app_name = "ServerApp",
    bin = [":server"],
    visibility = [
        "//score/message_passing/test/client_server_communication_one_way:__subpackages__",
    ],
)
