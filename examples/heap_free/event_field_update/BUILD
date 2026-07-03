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
    "@score_communication//score/mw/com",
    "@score_baselibs//score/mw/log",
]

# Skeleton: event + field update with no heap in the operational phase.
cc_binary(
    name = "skeleton",
    srcs = ["skeleton.cpp"],
    args = ["$(rootpath etc/mw_com_config.json)"],
    data = [
        "etc/logging.json",
        "etc/mw_com_config.json",
    ],
    env = {"MW_LOG_CONFIG_FILE": "$(location etc/logging.json)"},
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    deps = COM_DEP,
)

validate_json_schema_test(
    name = "validate_mw_com_config_schema",
    json = "etc/mw_com_config.json",
    schema = "@score_communication//score/mw/com:config_schema",
)

# Proxy: polling receive via GetNewSamples for event and field, no heap
# in the operational phase. Run alongside the skeleton.
cc_binary(
    name = "proxy",
    srcs = ["proxy.cpp"],
    args = ["$(rootpath etc/mw_com_config.json)"],
    data = [
        "etc/logging.json",
        "etc/mw_com_config.json",
    ],
    env = {"MW_LOG_CONFIG_FILE": "$(location etc/logging.json)"},
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    deps = COM_DEP,
)
