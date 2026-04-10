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
load("//score/mw/com/test:pkg_application.bzl", "pkg_application")
load("//score/mw/com/test/methods/methods_test_resources:config_generator_rule.bzl", "make_default_configs")

make_default_configs(
    name = "qm_qm_mw_com_config",
    asil_level_app = "QM",
    asil_level_communication_partner = "QM",
)

make_default_configs(
    name = "qm_asil_b_mw_com_config",
    asil_level_app = "QM",
    asil_level_communication_partner = "B",
)

make_default_configs(
    name = "asil_b_qm_mw_com_config",
    asil_level_app = "B",
    asil_level_communication_partner = "QM",
)

make_default_configs(
    name = "asil_b_asil_b_mw_com_config",
    asil_level_app = "B",
    asil_level_communication_partner = "B",
)

cc_library(
    name = "common_resources",
    srcs = ["common_resources.cpp"],
    hdrs = ["common_resources.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//score/mw/com/test/methods:__subpackages__"],
    deps = [
        "//score/mw/com/test/common_test_resources:command_line_parser",
        "//score/mw/com/test/common_test_resources:fail_test",
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
        "//score/mw/com/test/common_test_resources:assert_handler",
        "//score/mw/com/test/methods/methods_test_resources:provider_runner",
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
        "//score/mw/com/test/common_test_resources:assert_handler",
        "//score/mw/com/test/methods/methods_test_resources:consumer_runner",
    ],
)

# --- Packages ---

pkg_application(
    name = "provider-pkg",
    app_name = "ProviderApp",
    bin = [":provider_main"],
    etc = [
        ":qm_qm_mw_com_config",
        ":qm_asil_b_mw_com_config",
        ":asil_b_qm_mw_com_config",
        ":asil_b_asil_b_mw_com_config",
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
        ":qm_qm_mw_com_config",
        ":qm_asil_b_mw_com_config",
        ":asil_b_qm_mw_com_config",
        ":asil_b_asil_b_mw_com_config",
    ],
    visibility = [
        "//score/mw/com/test/methods/mixed_criticality/integration_test:__pkg__",
    ],
)

test_suite(
    name = "component_tests",
    tests = [
        "//score/mw/com/test/methods/mixed_criticality/integration_test:component_tests",
    ],
)
