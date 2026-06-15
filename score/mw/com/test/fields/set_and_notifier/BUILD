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
load("//bazel/tools:json_schema_validator.bzl", "validate_json_schema_test")
load("//score/mw/com/test:pkg_application.bzl", "pkg_application")

validate_json_schema_test(
    name = "validate_lola_schema",
    json = "config/mw_com_config.json",
    schema = "//score/mw/com:config_schema",
    tags = ["lint"],
)

validate_json_schema_test(
    name = "validate_initial_only_field_lola_schema",
    json = "config/initial_only_field_mw_com_config.json",
    schema = "//score/mw/com:config_schema",
    tags = ["lint"],
)

cc_binary(
    name = "provider",
    srcs = ["main_provider.cpp"],
    data = [
        "config/initial_only_field_mw_com_config.json",
        "config/mw_com_config.json",
    ],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    visibility = ["//score/mw/com/test/fields:__pkg__"],
    deps = [
        "//score/mw/com",
        "//score/mw/com/test/common_test_resources:command_line_parser",
        "//score/mw/com/test/common_test_resources:fail_test",
        "//score/mw/com/test/common_test_resources:stop_token_sig_term_handler",
        "//score/mw/com/test/fields/set_and_notifier/fields_test_resources:field_provider",
        "@score_baselibs//score/mw/log",
    ],
)

cc_binary(
    name = "consumer",
    srcs = ["main_consumer.cpp"],
    data = [
        "config/initial_only_field_mw_com_config.json",
        "config/mw_com_config.json",
    ],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    visibility = ["//score/mw/com/test/fields:__pkg__"],
    deps = [
        "//score/mw/com",
        "//score/mw/com/test/common_test_resources:command_line_parser",
        "//score/mw/com/test/common_test_resources:fail_test",
        "//score/mw/com/test/fields/set_and_notifier/fields_test_resources:field_consumer",
        "@score_baselibs//score/mw/log",
    ],
)

pkg_application(
    name = "provider-pkg",
    app_name = "MainProviderApp",
    bin = [":provider"],
    etc = [
        "config/initial_only_field_mw_com_config.json",
        "config/logging.json",
        "config/mw_com_config.json",
    ],
    visibility = [
        "//platform/aas/test/mw/com:__pkg__",
        "//score/mw/com/test/fields/set_and_notifier:__subpackages__",
    ],
)

pkg_application(
    name = "consumer-pkg",
    app_name = "MainConsumerApp",
    bin = [":consumer"],
    etc = [
        "config/initial_only_field_mw_com_config.json",
        "config/logging.json",
        "config/mw_com_config.json",
    ],
    visibility = [
        "//platform/aas/test/mw/com:__pkg__",
        "//score/mw/com/test/fields/set_and_notifier:__subpackages__",
    ],
)
