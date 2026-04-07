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

cc_library(
    name = "test_method_datatype",
    srcs = ["test_method_datatype.cpp"],
    hdrs = ["test_method_datatype.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com/test/methods/edge_cases_test:__subpackages__",
    ],
    deps = [
        "//score/mw/com",
    ],
)

cc_binary(
    name = "edge_cases_test",
    srcs = ["edge_cases_test.cpp"],
    data = ["config/mw_com_config.json"],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    deps = [
        "//score/mw/com",
        "//score/mw/com/test/common_test_resources:command_line_parser",
        "//score/mw/com/test/common_test_resources:fail_test",
        "//score/mw/com/test/common_test_resources:assert_handler",
        "//score/mw/com/test/methods/edge_cases_test:test_method_datatype",
        "//score/mw/com/test/methods/methods_test_resources:proxy_container",
        "@score_baselibs//score/language/futurecpp",
    ],
)

pkg_application(
    name = "edge-case-test-pkg",
    app_name = "EdgeCasesTestApp",
    bin = [":edge_cases_test"],
    etc = [
        "config/mw_com_config.json",
    ],
    visibility = [
        "//score/mw/com/test/methods/edge_cases_test/integration_test:__subpackages__",
    ],
)

test_suite(
    name = "component_tests",
    tests = [
        "//score/mw/com/test/methods/edge_cases_test/integration_test:component_tests",
    ],
)
