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

load("@score_baselibs//score/language/safecpp:toolchain_features.bzl", "COMPILER_WARNING_FEATURES")
load("//bazel/tools:json_schema_validator.bzl", "validate_json_schema_test")
load("//score/mw/com/test:pkg_application.bzl", "pkg_application")

validate_json_schema_test(
    name = "validate_config_schema_asil_b_to_asil_b",
    json = "config/asil_b_to_asil_b_mw_com_config.json",
    schema = "//score/mw/com:config_schema",
    tags = ["lint"],
)

validate_json_schema_test(
    name = "validate_config_schema_asil_b_to_qm",
    json = "config/consumer_asil_b_to_qm_mw_com_config.json",
    schema = "//score/mw/com:config_schema",
    tags = ["lint"],
)

validate_json_schema_test(
    name = "validate_config_schema_qm_to_qm",
    json = "config/qm_to_qm_mw_com_config.json",
    schema = "//score/mw/com:config_schema",
    tags = ["lint"],
)

cc_library(
    name = "common_resources",
    srcs = ["common_resources.cpp"],
    hdrs = ["common_resources.h"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        "//score/mw/com",
        "//score/mw/com/test/common_test_resources:command_line_parser",
    ],
)

cc_library(
    name = "provider",
    srcs = ["provider.cpp"],
    hdrs = ["provider.h"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":common_resources",
        "//score/mw/com",
        "//score/mw/com/test/common_test_resources:fail_test",
        "//score/mw/com/test/common_test_resources:process_synchronizer",
        "//score/mw/com/test/common_test_resources:skeleton_container",
        "//score/mw/com/test/fields/mixed_criticality/datatypes:field_datatypes",
    ],
)

cc_library(
    name = "consumer",
    srcs = ["consumer.cpp"],
    hdrs = ["consumer.h"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":common_resources",
        "//score/mw/com",
        "//score/mw/com/test/common_test_resources:fail_test",
        "//score/mw/com/test/common_test_resources:process_synchronizer",
        "//score/mw/com/test/common_test_resources:proxy_container",
        "//score/mw/com/test/common_test_resources:proxy_event_receiver",
        "//score/mw/com/test/common_test_resources:proxy_event_state_change_notifier",
        "//score/mw/com/test/fields/mixed_criticality/datatypes:field_datatypes",
        "//score/mw/com/test/fields/test_resources:getter_and_setter_checkers",
    ],
)

cc_binary(
    name = "provider_main",
    srcs = ["provider_main.cpp"],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    deps = [
        ":common_resources",
        ":provider",
        "//score/mw/com",
        "//score/mw/com/test/common_test_resources:assert_handler",
        "//score/mw/com/test/common_test_resources:stop_token_sig_term_handler",
    ],
)

cc_binary(
    name = "consumer_main",
    srcs = ["consumer_main.cpp"],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    deps = [
        ":common_resources",
        ":consumer",
        "//score/mw/com",
        "//score/mw/com/test/common_test_resources:assert_handler",
        "//score/mw/com/test/common_test_resources:stop_token_sig_term_handler",
    ],
)

pkg_application(
    name = "provider-pkg",
    app_name = "ProviderApp",
    bin = [":provider_main"],
    etc = [
        "config/asil_b_to_asil_b_mw_com_config.json",
        "config/consumer_asil_b_to_qm_mw_com_config.json",
        "config/qm_to_qm_mw_com_config.json",
        "config/logging.json",
    ],
    visibility = [
        "//score/mw/com/test/fields/mixed_criticality/integration_test:__pkg__",
    ],
)

pkg_application(
    name = "consumer-pkg",
    app_name = "ConsumerApp",
    bin = [":consumer_main"],
    etc = [
        "config/asil_b_to_asil_b_mw_com_config.json",
        "config/consumer_asil_b_to_qm_mw_com_config.json",
        "config/qm_to_qm_mw_com_config.json",
        "config/logging.json",
    ],
    visibility = [
        "//score/mw/com/test/fields/mixed_criticality/integration_test:__pkg__",
    ],
)
