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

cc_binary(
    name = "disabled_method_test",
    srcs = ["disabled_method_test.cpp"],
    data = ["config/disabled_methods_mw_com_config.json"],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    deps = [
        "//score/mw/com",
        "//score/mw/com/test/common_test_resources:assert_handler",
        "//score/mw/com/test/common_test_resources:fail_test",
        "//score/mw/com/test/common_test_resources:proxy_container",
        "//score/mw/com/test/methods/methods_test_resources:common_resources",
        "//score/mw/com/test/methods/methods_test_resources/all_signatures_datatype",
        "//score/mw/com/test/methods/methods_test_resources/all_signatures_datatype:all_signatures_method_provider",
        "@score_baselibs//score/language/futurecpp",
    ],
)

cc_binary(
    name = "incomplete_handlers_test",
    srcs = ["incomplete_handlers_test.cpp"],
    data = ["config/mw_com_config.json"],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    deps = [
        "//score/mw/com",
        "//score/mw/com/test/common_test_resources:assert_handler",
        "//score/mw/com/test/common_test_resources:fail_test",
        "//score/mw/com/test/methods/methods_test_resources:common_resources",
        "//score/mw/com/test/methods/methods_test_resources/all_signatures_datatype:all_signatures_method_provider",
        "@score_baselibs//score/language/futurecpp",
    ],
)

cc_binary(
    name = "proxy_recreation_test",
    srcs = ["proxy_recreation_test.cpp"],
    data = ["config/mw_com_config.json"],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    deps = [
        "//score/mw/com",
        "//score/mw/com/test/common_test_resources:assert_handler",
        "//score/mw/com/test/methods/methods_test_resources:common_resources",
        "//score/mw/com/test/methods/methods_test_resources/all_signatures_datatype:all_signatures_method_consumer",
        "//score/mw/com/test/methods/methods_test_resources/all_signatures_datatype:all_signatures_method_provider",
        "@score_baselibs//score/language/futurecpp",
    ],
)

cc_binary(
    name = "skeleton_recreation_test",
    srcs = ["skeleton_recreation_test.cpp"],
    data = ["config/mw_com_config.json"],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    deps = [
        "//score/mw/com",
        "//score/mw/com/test/common_test_resources:assert_handler",
        "//score/mw/com/test/common_test_resources:stop_token_sig_term_handler",
        "//score/mw/com/test/methods/methods_test_resources:common_resources",
        "//score/mw/com/test/methods/methods_test_resources/all_signatures_datatype:all_signatures_method_consumer",
        "//score/mw/com/test/methods/methods_test_resources/all_signatures_datatype:all_signatures_method_provider",
        "@score_baselibs//score/language/futurecpp",
    ],
)

pkg_application(
    name = "edge-case-test-pkg",
    app_name = "EdgeCasesTestApp",
    bin = [
        ":disabled_method_test",
        ":incomplete_handlers_test",
        ":proxy_recreation_test",
        ":skeleton_recreation_test",
    ],
    etc = [
        "config/disabled_methods_mw_com_config.json",
        "config/logging.json",
        "config/mw_com_config.json",
    ],
    visibility = [
        "//score/mw/com/test/methods/edge_cases_test/integration_test:__subpackages__",
    ],
)
