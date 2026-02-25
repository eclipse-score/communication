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
    name = "validate_lola_service_schema",
    json = "config/service/mw_com_config.json",
    schema = "//score/mw/com:config_schema",
    tags = ["lint"],
)

validate_json_schema_test(
    name = "validate_lola_client_schema",
    json = "config/client/mw_com_config.json",
    schema = "//score/mw/com:config_schema",
    tags = ["lint"],
)

cc_library(
    name = "test_datatype",
    srcs = ["test_datatype.cpp"],
    hdrs = ["test_datatype.h"],
    features = COMPILER_WARNING_FEATURES,
    deps = ["//score/mw/com"],
)

cc_binary(
    name = "service",
    srcs = ["service.cpp"],
    data = ["config/service/mw_com_config.json"],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    visibility = ["//score/mw/com/test/find_all_semantics:__pkg__"],
    deps = [
        ":test_datatype",
        "//score/mw/com",
        "//score/mw/com/test/common_test_resources:sctf_test_runner",
        "@score_baselibs//score/result",
        "@score_logging//score/mw/log",
    ],
)

cc_binary(
    name = "client",
    srcs = ["client.cpp"],
    data = ["config/client/mw_com_config.json"],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    visibility = ["//score/mw/com/test/find_all_semantics:__pkg__"],
    deps = [
        ":test_datatype",
        "//score/mw/com",
        "//score/mw/com/test/common_test_resources:sctf_test_runner",
        "@score_logging//score/mw/log",
    ],
)

pkg_application(
    name = "service-pkg",
    app_name = "ServiceApp",
    bin = [":service"],
    etc = [
        "config/service/mw_com_config.json",
        "config/logging.json",
    ],
    visibility = [
        "//platform/aas/test/mw/com:__pkg__",
        "//score/mw/com/test/find_any_semantics:__subpackages__",
    ],
)

pkg_application(
    name = "client-pkg",
    app_name = "ClientApp",
    bin = [":client"],
    etc = [
        "config/client/mw_com_config.json",
        "config/logging.json",
    ],
    visibility = [
        "//platform/aas/test/mw/com:__pkg__",
        "//score/mw/com/test/find_any_semantics:__subpackages__",
    ],
)
