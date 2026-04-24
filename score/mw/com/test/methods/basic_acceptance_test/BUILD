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
    name = "validate_config_schema",
    json = "config/mw_com_config.json",
    schema = "//score/mw/com:config_schema",
    tags = ["lint"],
)

cc_library(
    name = "test_method_datatype",
    srcs = ["test_method_datatype.cpp"],
    hdrs = ["test_method_datatype.h"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        "//score/mw/com",
    ],
)

cc_library(
    name = "consumer",
    srcs = ["consumer.cpp"],
    hdrs = ["consumer.h"],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = [
        ":test_method_datatype",
        "//score/mw/com",
        "//score/mw/com/test/methods/methods_test_resources:method_consumer",
        "//score/mw/com/test/methods/methods_test_resources:process_synchronizer",
    ],
)

cc_library(
    name = "provider",
    srcs = ["provider.cpp"],
    hdrs = ["provider.h"],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = [
        ":test_method_datatype",
        "//score/mw/com",
        "//score/mw/com/test/methods/methods_test_resources:method_provider",
        "//score/mw/com/test/methods/methods_test_resources:process_synchronizer",
    ],
    deps = [
        "@score_baselibs//score/language/futurecpp",
    ],
)

cc_binary(
    name = "main_provider",
    srcs = ["main_provider.cpp"],
    data = ["config/mw_com_config.json"],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    deps = [
        ":provider",
        "//score/mw/com",
        "//score/mw/com/test/common_test_resources:assert_handler",
        "//score/mw/com/test/common_test_resources:stop_token_sig_term_handler",
    ],
)

cc_binary(
    name = "main_consumer",
    srcs = ["main_consumer.cpp"],
    data = ["config/mw_com_config.json"],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    deps = [
        ":consumer",
        "//score/mw/com",
        "//score/mw/com/test/common_test_resources:assert_handler",
    ],
)

cc_binary(
    name = "main_consumer_and_provider",
    srcs = ["main_consumer_and_provider.cpp"],
    data = ["config/mw_com_config.json"],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    deps = [
        ":consumer",
        ":provider",
        "//score/mw/com",
        "//score/mw/com/test/common_test_resources:assert_handler",
        "//score/mw/com/test/common_test_resources:stop_token_sig_term_handler",
    ],
)

pkg_application(
    name = "main_provider-pkg",
    app_name = "MainProviderApp",
    bin = [":main_provider"],
    etc = [
        "config/mw_com_config.json",
        "config/logging.json",
    ],
    visibility = [
        "//score/mw/com/test/methods/basic_acceptance_test:__subpackages__",
    ],
)

pkg_application(
    name = "main_consumer-pkg",
    app_name = "MainConsumerApp",
    bin = [":main_consumer"],
    etc = [
        "config/mw_com_config.json",
        "config/logging.json",
    ],
    visibility = [
        "//score/mw/com/test/methods/basic_acceptance_test:__subpackages__",
    ],
)

pkg_application(
    name = "main_consumer_and_provider-pkg",
    app_name = "MainConsumerAndProviderApp",
    bin = [":main_consumer_and_provider"],
    etc = [
        "config/mw_com_config.json",
        "config/logging.json",
    ],
    visibility = [
        "//score/mw/com/test/methods/basic_acceptance_test:__subpackages__",
    ],
)

test_suite(
    name = "component_tests",
    tests = [],
    visibility = ["//score/mw/com/test/methods:__pkg__"],
)
