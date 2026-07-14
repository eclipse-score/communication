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
load("@score_communication//score/mw/com/test:pkg_application.bzl", "pkg_application")

cc_library(
    name = "service_interface",
    srcs = ["long_running_service.cpp"],
    hdrs = ["long_running_service.h"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        "@score_communication//score/mw/com",
    ],
)

cc_binary(
    name = "provider",
    srcs = [
        "provider/provider.cpp",
        "provider/provider.h",
    ],
    data = ["provider/mw_com_config.json"],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    deps = [
        ":service_interface",
        "@score_communication//score/mw/com",
    ],
)

cc_binary(
    name = "consumer",
    srcs = [
        "consumer/consumer.cpp",
        "consumer/consumer.h",
    ],
    data = ["consumer/mw_com_config.json"],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    deps = [
        ":service_interface",
        "@score_communication//score/mw/com",
    ],
)

pkg_application(
    name = "provider-pkg",
    app_name = "LongRunningServer",
    bin = [":provider"],
    etc = [
        "provider/logging.json",
        "provider/mw_com_config.json",
    ],
    visibility = [
        "//score/mw/com/doc:__subpackages__",
    ],
)

pkg_tar(
    name = "provider-tar",
    srcs = [":provider-pkg"],
    visibility = [
        "//score/mw/com/doc:__subpackages__",
    ],
)

pkg_application(
    name = "consumer-pkg",
    app_name = "LongRunningClient",
    bin = [":consumer"],
    etc = [
        "consumer/logging.json",
        "consumer/mw_com_config.json",
    ],
    visibility = [
        "//score/mw/com/doc:__subpackages__",
    ],
)

pkg_tar(
    name = "consumer-tar",
    srcs = [":consumer-pkg"],
    visibility = [
        "//score/mw/com/doc:__subpackages__",
    ],
)

filegroup(
    name = "chapter_11",
    srcs = [
        ":consumer-tar",
        ":provider-tar",
    ],
    visibility = ["//score/mw/com/doc:__subpackages__"],
)
