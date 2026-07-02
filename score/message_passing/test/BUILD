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

load("@rules_cc//cc:defs.bzl", "cc_binary")
load("@score_baselibs//score/language/safecpp:toolchain_features.bzl", "COMPILER_WARNING_FEATURES")
load("//score/mw/com/test:pkg_application.bzl", "pkg_application")

cc_binary(
    name = "client",
    srcs = [
        "client.cpp",
        "common.h",
    ],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    visibility = ["//score/message_passing/test:__subpackages__"],
    deps = [
        "@score_communication//score/message_passing",
    ],
)

cc_binary(
    name = "server",
    srcs = [
        "common.h",
        "server.cpp",
    ],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    visibility = ["//score/message_passing/test:__subpackages__"],
    deps = [
        "@score_communication//score/message_passing",
    ],
)

pkg_application(
    name = "client-pkg",
    app_name = "client",
    bin = [":client"],
    visibility = [
        "//score/message_passing/test:__subpackages__",
    ],
)

pkg_application(
    name = "server-pkg",
    app_name = "server",
    bin = [":server"],
    visibility = [
        "//score/message_passing/test:__subpackages__",
    ],
)
