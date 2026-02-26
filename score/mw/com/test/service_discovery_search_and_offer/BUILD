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
    name = "test_datatype",
    srcs = ["test_datatype.cpp"],
    hdrs = ["test_datatype.h"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        "//score/mw/com",
        "//score/mw/com/test/common_test_resources:test_interface",
    ],
)

cc_binary(
    name = "service",
    srcs = ["service.cpp"],
    data = ["config/mw_com_config.json"],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    deps = [
        ":test_datatype",
        "//score/mw/com",
        "//score/mw/com/test/common_test_resources:common_service",
        "//score/mw/com/test/common_test_resources:sctf_test_runner",
        "//score/mw/com/test/common_test_resources:sync_utils",
        "@score_logging//score/mw/log",
    ],
)

cc_binary(
    name = "client",
    srcs = ["client.cpp"],
    data = ["config/mw_com_config.json"],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    deps = [
        ":test_datatype",
        "//score/mw/com",
        "//score/mw/com/test/common_test_resources:proxy_observer",
        "//score/mw/com/test/common_test_resources:sctf_test_runner",
        "//score/mw/com/test/common_test_resources:sync_utils",
        "@score_logging//score/mw/log",
    ],
)

pkg_application(
    name = "service-pkg",
    app_name = "ServiceApp",
    bin = [":service"],
    etc = [
        "config/mw_com_config.json",
        "config/logging.json",
    ],
    visibility = [
        "//platform/aas/test/mw/com:__pkg__",
        "//score/mw/com/test:__pkg__",
        "//score/mw/com/test/service_discovery_search_and_offer:__subpackages__",
    ],
)

pkg_application(
    name = "client-pkg",
    app_name = "ClientApp",
    bin = [":client"],
    etc = [
        "config/mw_com_config.json",
        "config/logging.json",
    ],
    visibility = [
        "//platform/aas/test/mw/com:__pkg__",
        "//score/mw/com/test:__pkg__",
        "//score/mw/com/test/service_discovery_search_and_offer:__subpackages__",
    ],
)
