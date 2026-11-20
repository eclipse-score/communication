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
    name = "api_idempotency_datatype",
    srcs = ["api_idempotency_datatype.cpp"],
    hdrs = ["api_idempotency_datatype.h"],
    features = COMPILER_WARNING_FEATURES,
    deps = ["//score/mw/com"],
)

cc_binary(
    name = "main_api_idempotency",
    srcs = ["main_api_idempotency.cpp"],
    data = ["config/mw_com_config.json"],
    features = COMPILER_WARNING_FEATURES + ["aborts_upon_exception"],
    deps = [
        ":api_idempotency_datatype",
        "//score/mw/com",
        "//score/mw/com/test/common_test_resources:assert_handler",
        "//score/mw/com/test/common_test_resources:fail_test",
        "//score/mw/com/test/common_test_resources:service_instance_manifest_parser",
        "//score/mw/com/test/common_test_resources:skeleton_container",
        "@score_baselibs//score/result",
    ],
)

pkg_application(
    name = "main_api_idempotency-pkg",
    app_name = "ApiIdempotencyApp",
    bin = [":main_api_idempotency"],
    etc = [
        "config/logging.json",
        "config/mw_com_config.json",
    ],
    visibility = [
        "//score/mw/com/test/api_idempotency:__subpackages__",
    ],
)
