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
load("//quality/unit_testing:unit_testing.bzl", "cc_unit_test")
load("//score/mw/com/test:pkg_application.bzl", "pkg_application")

cc_library(
    name = "service_interface",
    srcs = ["hello_world_service.cpp"],
    hdrs = ["hello_world_service.h"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        "//score/mw/com",
    ],
)

cc_binary(
    name = "provider_app",
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
        ":skeleton_component",
        "//score/mw/com",
    ],
)

cc_library(
    name = "skeleton_component",
    srcs = [
        "provider/skeleton_component.cpp",
        "provider/skeleton_component.h",
    ],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    deps = [
        ":service_interface",
        "//score/mw/com",
    ],
)

cc_library(
    name = "proxy_component",
    srcs = [
        "consumer/proxy_component.cpp",
        "consumer/proxy_component.h",
    ],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    deps = [
        ":service_interface",
        "//score/mw/com",
    ],
)

cc_binary(
    name = "consumer_app",
    srcs = [
        "consumer/consumer.cpp",
        "consumer/consumer.h",
    ],
    data = ["consumer/mw_com_config.json"],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    deps = [
        ":proxy_component",
        ":service_interface",
        "//score/mw/com",
    ],
)

pkg_application(
    name = "provider-pkg",
    app_name = "HelloWorldServer",
    bin = [":provider_app"],
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
    app_name = "HelloWorldClient",
    bin = [":consumer_app"],
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

cc_unit_test(
    name = "proxy_component_test",
    srcs = ["consumer/proxy_component_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":proxy_component",
        "//score/mw/com:test_types",
    ],
)

cc_unit_test(
    name = "skeleton_component_test",
    srcs = ["provider/skeleton_component_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":skeleton_component",
        "//score/mw/com:test_types",
    ],
)

filegroup(
    name = "chapter_12",
    srcs = [
        ":consumer-tar",
        ":provider-tar",
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
