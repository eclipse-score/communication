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

validate_json_schema_test(
    name = "validate_lola_schema",
    json = "config/mw_com_config.json",
    schema = "//score/mw/com:config_schema",
    tags = ["lint"],
)

cc_library(
    name = "common_resources",
    srcs = ["common_resources.cpp"],
    hdrs = [
        "common_resources.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        "//score/mw/com/test/common_test_resources:command_line_parser",
        "//score/mw/com/test/common_test_resources:fail_test",
        "//score/mw/com/test/fields/getter_setter_if_available/datatypes:field_datatypes",
    ],
)

cc_library(
    name = "field_consumer",
    srcs = ["consumer.cpp"],
    hdrs = ["consumer.h"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":common_resources",
        "//score/mw/com",
        "//score/mw/com/test/common_test_resources:fail_test",
        "//score/mw/com/test/common_test_resources:process_synchronizer",
        "//score/mw/com/test/common_test_resources:proxy_container",
        "//score/mw/com/test/fields/test_resources:getter_and_setter_checkers",
    ],
)

cc_library(
    name = "field_provider",
    srcs = ["provider.cpp"],
    hdrs = ["provider.h"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":common_resources",
        "//score/mw/com",
        "//score/mw/com/test/common_test_resources:fail_test",
        "//score/mw/com/test/common_test_resources:process_synchronizer",
        "//score/mw/com/test/common_test_resources:skeleton_container",
    ],
)

cc_binary(
    name = "provider",
    srcs = ["main_provider.cpp"],
    data = [
        "config/logging.json",
        "config/mw_com_config.json",
    ],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    visibility = ["//score/mw/com/test/fields:__pkg__"],
    deps = [
        ":common_resources",
        ":field_provider",
        "//score/mw/com",
        "//score/mw/com/test/common_test_resources:stop_token_sig_term_handler",
        "@score_baselibs//score/mw/log",
    ],
)

cc_binary(
    name = "consumer",
    srcs = ["main_consumer.cpp"],
    data = [
        "config/logging.json",
        "config/mw_com_config.json",
    ],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    visibility = ["//score/mw/com/test/fields:__pkg__"],
    deps = [
        ":common_resources",
        ":field_consumer",
        "//score/mw/com",
        "//score/mw/com/test/common_test_resources:stop_token_sig_term_handler",
        "@score_baselibs//score/mw/log",
    ],
)

cc_binary(
    name = "consumer_and_provider",
    srcs = ["main_consumer_and_provider.cpp"],
    data = [
        "config/logging.json",
        "config/mw_com_config.json",
    ],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    visibility = ["//score/mw/com/test/fields:__pkg__"],
    deps = [
        ":common_resources",
        ":field_consumer",
        ":field_provider",
        "//score/mw/com",
        "//score/mw/com/test/common_test_resources:stop_token_sig_term_handler",
        "@score_baselibs//score/mw/log",
    ],
)

pkg_application(
    name = "provider-pkg",
    app_name = "MainProviderApp",
    bin = [":provider"],
    etc = [
        "config/logging.json",
        "config/mw_com_config.json",
    ],
    visibility = [
        "//platform/aas/test/mw/com:__pkg__",
        "//score/mw/com/test/fields/getter_setter_if_available:__subpackages__",
    ],
)

pkg_application(
    name = "consumer-pkg",
    app_name = "MainConsumerApp",
    bin = [":consumer"],
    etc = [
        "config/logging.json",
        "config/mw_com_config.json",
    ],
    visibility = [
        "//platform/aas/test/mw/com:__pkg__",
        "//score/mw/com/test/fields/getter_setter_if_available:__subpackages__",
    ],
)

pkg_application(
    name = "consumer_and_provider-pkg",
    app_name = "MainConsumerAndProviderApp",
    bin = [":consumer_and_provider"],
    etc = [
        "config/logging.json",
        "config/mw_com_config.json",
    ],
    visibility = [
        "//platform/aas/test/mw/com:__pkg__",
        "//score/mw/com/test/fields/getter_setter_if_available:__subpackages__",
    ],
)
