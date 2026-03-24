# *******************************************************************************
# Copyright (c) 2025 Contributors to the Eclipse Foundation
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

load("@score_baselibs//:bazel/unit_tests.bzl", "cc_unit_test_suites_for_host_and_qnx")
load("@score_baselibs//score/quality/clang_tidy:extra_checks.bzl", "clang_tidy_extra_checks")

package(default_visibility = [
    "//score/memory:__subpackages__",
])

alias(
    name = "string_literal",
    actual = "@score_baselibs//score/string:string_literal",
    deprecation = "Use @score_baselibs//score/string:string_literal directly. This alias will be removed.",
    visibility = ["//visibility:public"],
)

alias(
    name = "string_comparison_adaptor",
    actual = "@score_baselibs//score/string:string_comparison_adaptor",
    deprecation = "Use @score_baselibs//score/string:string_comparison_adaptor directly. This alias will be removed.",
    visibility = ["//visibility:public"],
)

alias(
    name = "split_string_view",
    actual = "@score_baselibs//score/string:split_string_view",
    deprecation = "Use @score_baselibs//score/string:split_string_view directly. This alias will be removed.",
    visibility = ["//visibility:public"],
)

cc_unit_test_suites_for_host_and_qnx(
    name = "unit_test_suite",
    test_suites_from_sub_packages = [
        "//score/memory/shared:unit_test_suite",
    ],
    visibility = [
        "//visibility:public",
    ],
)

clang_tidy_extra_checks(
    name = "clang_tidy_extra_checks",
    extra_features = [
        "spp_code_style_check_header_guards",
    ],
    tidy_config_file = ".clang-tidy-extra",
)
