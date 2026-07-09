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
load("//bazel:json_schema_validator.bzl", "validate_json_schema_test")

COM_DEP = [
    "//heap_free:heap_check_lib",
    "//heap_free:sensor_interface",
    "@score_baselibs//score/concurrency:notification",
    "@score_baselibs//score/language/futurecpp",
    "@score_baselibs//score/mw/log",
    "@score_communication//score/mw/com",
]

cc_binary(
    name = "application_a",
    srcs = ["application_a.cpp"],
    args = ["$(rootpath etc/application_a_mw_com_config.json)"],
    data = [
        "etc/application_a_logging.json",
        "etc/application_a_mw_com_config.json",
    ],
    env = {"MW_LOG_CONFIG_FILE": "$(location etc/application_a_logging.json)"},
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    deps = COM_DEP,
)

cc_binary(
    name = "application_b",
    srcs = ["application_b.cpp"],
    args = ["$(rootpath etc/application_b_mw_com_config.json)"],
    data = [
        "etc/application_b_logging.json",
        "etc/application_b_mw_com_config.json",
    ],
    env = {"MW_LOG_CONFIG_FILE": "$(location etc/application_b_logging.json)"},
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    deps = COM_DEP,
)

validate_json_schema_test(
    name = "validate_application_a_mw_com_config_schema",
    json = "etc/application_a_mw_com_config.json",
    schema = "@score_communication//score/mw/com:config_schema",
    tags = ["lint"],
)

validate_json_schema_test(
    name = "validate_application_b_mw_com_config_schema",
    json = "etc/application_b_mw_com_config.json",
    schema = "@score_communication//score/mw/com:config_schema",
    tags = ["lint"],
)
