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

exports_files(
    ["config/logging.json"],
    visibility = ["//score/mw/com/test:__subpackages__"],
)

cc_library(
    name = "common_resources",
    srcs = ["common_resources.cpp"],
    hdrs = ["common_resources.h"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        "//score/mw/com/test/common_test_resources:command_line_parser",
        "//score/mw/com/test/common_test_resources:fail_test",
    ],
)

cc_library(
    name = "test_constants",
    srcs = ["test_constants.cpp"],
    hdrs = ["test_constants.h"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        "//score/mw/com",
    ],
)

cc_library(
    name = "consumer_impl",
    srcs = ["consumer.cpp"],
    hdrs = ["consumer.h"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":common_resources",
        ":test_constants",
        "//score/mw/com",
        "//score/mw/com/impl:instance_specifier",
        "//score/mw/com/test/common_test_resources:fail_test",
        "//score/mw/com/test/common_test_resources:process_synchronizer",
        "//score/mw/com/test/common_test_resources:proxy_container",
        "//score/mw/com/test/common_test_resources:proxy_event_receiver",
        "//score/mw/com/test/common_test_resources:proxy_event_state_change_notifier",
        "//score/mw/com/test/loading_add_on_configuration/types:example_interface",
    ],
)

cc_library(
    name = "provider_impl",
    srcs = ["provider.cpp"],
    hdrs = ["provider.h"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":common_resources",
        ":test_constants",
        "//score/mw/com",
        "//score/mw/com/impl:instance_specifier",
        "//score/mw/com/test/common_test_resources:fail_test",
        "//score/mw/com/test/common_test_resources:process_synchronizer",
        "//score/mw/com/test/common_test_resources:skeleton_container",
        "//score/mw/com/test/loading_add_on_configuration/types:example_interface",
    ],
)

cc_binary(
    name = "consumer",
    srcs = ["main_consumer.cpp"],
    data = [
        "config/mw_com_add_on_config.json",
        "config/mw_com_config.json",
    ],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    visibility = ["//score/mw/com/test/loading_add_on_configuration:__pkg__"],
    deps = [
        ":common_resources",
        ":consumer_impl",
        "//score/mw/com",
        "//score/mw/com/test/common_test_resources:fail_test",
        "//score/mw/com/test/common_test_resources:process_synchronizer",
        "//score/mw/com/test/common_test_resources:stop_token_sig_term_handler",
        "@score_baselibs//score/mw/log",
    ],
)

cc_binary(
    name = "provider",
    srcs = ["main_provider.cpp"],
    data = [
        "config/mw_com_add_on_config.json",
        "config/mw_com_config.json",
    ],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    visibility = ["//score/mw/com/test/loading_add_on_configuration:__pkg__"],
    deps = [
        ":common_resources",
        ":provider_impl",
        "//score/mw/com",
        "//score/mw/com/test/common_test_resources:fail_test",
        "//score/mw/com/test/common_test_resources:process_synchronizer",
        "//score/mw/com/test/common_test_resources:stop_token_sig_term_handler",
        "@score_baselibs//score/mw/log",
    ],
)

pkg_application(
    name = "consumer-pkg",
    app_name = "consumer",
    bin = [":consumer"],
    etc = [
        "config/logging.json",
        "config/mw_com_config.json",
        "config/mw_com_add_on_config.json",
    ],
    visibility = [
        "//platform/aas/test/mw/com:__pkg__",
        "//score/mw/com/test/loading_add_on_configuration:__subpackages__",
    ],
)

pkg_application(
    name = "provider-pkg",
    app_name = "provider",
    bin = [":provider"],
    etc = [
        "config/logging.json",
        "config/mw_com_config.json",
        "config/mw_com_add_on_config.json",
    ],
    visibility = [
        "//platform/aas/test/mw/com:__pkg__",
        "//score/mw/com/test/loading_add_on_configuration:__subpackages__",
    ],
)
