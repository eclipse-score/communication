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
    name = "consumer",
    srcs = ["consumer.cpp"],
    hdrs = ["consumer.h"],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = [
        "//score/mw/com",
        "//score/mw/com/test/common_test_resources:process_synchronizer",
        "//score/mw/com/test/methods/methods_test_resources/all_signatures_datatype",
        "//score/mw/com/test/methods/methods_test_resources/all_signatures_datatype:all_signatures_method_consumer",
    ],
    deps = [
        "@score_baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "provider",
    srcs = ["provider.cpp"],
    hdrs = ["provider.h"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        "//score/mw/com",
        "//score/mw/com/test/common_test_resources:process_synchronizer",
        "//score/mw/com/test/methods/methods_test_resources/all_signatures_datatype",
        "//score/mw/com/test/methods/methods_test_resources/all_signatures_datatype:all_signatures_method_provider",
    ],
)

cc_binary(
    name = "provider_main",
    srcs = ["provider_main.cpp"],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    deps = [
        ":provider",
        "//score/mw/com/test/common_test_resources:assert_handler",
        "//score/mw/com/test/common_test_resources:stop_token_sig_term_handler",
        "//score/mw/com/test/methods/methods_test_resources:common_resources",
    ],
)

cc_binary(
    name = "consumer_main",
    srcs = ["consumer_main.cpp"],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    deps = [
        ":consumer",
        "//score/mw/com/test/common_test_resources:assert_handler",
        "//score/mw/com/test/methods/methods_test_resources:common_resources",
    ],
)

# --- Packages ---

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
        "//score/mw/com/test/methods/mixed_criticality/integration_test:__pkg__",
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
        "//score/mw/com/test/methods/mixed_criticality/integration_test:__pkg__",
    ],
)
