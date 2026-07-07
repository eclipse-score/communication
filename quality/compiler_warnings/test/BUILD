# *******************************************************************************
# Copyright (c) 2025 Contributors to the Eclipse Foundation
#
# See the NOTICE file(s) distributed with this work for additional
# information regarding copyright ownership.
#
# This program and the accompanying materials are made available under the
# terms of the Apache License Version 2.0 which is available at
# https://apache.org
#
# SPDX-License-Identifier: Apache-2.0
# *******************************************************************************

load("@bazel_skylib//rules:build_test.bzl", "build_test")
load("@rules_build_error//lang/cc:defs.bzl", "cc_build_error_test")
load("@rules_build_error//matcher:defs.bzl", "matcher")
load("//score/mw:common_features.bzl", "COMPILER_WARNING_FEATURES")

# Compiler warning feature tests.
WARNING_FLAGS_BY_CASE = {
    "conversion": ["-Wfloat-conversion"],
    "shadow": ["-Wshadow"],
    "sign_compare": ["-Wsign-compare"],
    "unused_parameter": ["-Wunused-parameter"],
    "cast_qual": ["-Wcast-qual"],
    "format_nonliteral": ["-Wformat-nonliteral"],
    "undef": ["-Wundef"],
    "delete_non_virtual_dtor": ["-Wdelete-non-virtual-dtor"],
    "overloaded_virtual": ["-Woverloaded-virtual"],
}

# Case 1: warning code should compile when warnings are not treated as errors.
[
    cc_library(
        name = "tp_{}".format(case),
        srcs = ["has_warnings/{}.cpp".format(case)],
        copts = warning,
        features = ["-score_communication_treat_warnings_as_errors"],
        tags = ["manual"],
    )
    for case, warning in WARNING_FLAGS_BY_CASE.items()
]

build_test(
    name = "tp_build_test",
    targets = [":tp_{}".format(case) for case, _ in WARNING_FLAGS_BY_CASE.items()],
)

# Case 2: clean code should compile with warning features enabled.
cc_library(
    name = "fp_all",
    srcs = ["no_warnings/clean.cpp"],
    features = COMPILER_WARNING_FEATURES,
)

build_test(
    name = "fp_build_test",
    targets = [":fp_all"],
)

# Case 3: warning code should fail when built with -Werror.
[
    cc_build_error_test(
        name = "fail_{}_build_error_test".format(case),
        src = "has_warnings/{}.cpp".format(case),
        compile_stderr = matcher.contains_extended_regex(
            # Convert e.g. -Wsign-compare to a flexible regex for stderr matching.
            "Werror.*{}".format(
                warning[0][2:].replace("-", ".*"),
            ),
        ),
        copts = ["-Werror"] + warning,
    )
    for case, warning in WARNING_FLAGS_BY_CASE.items()
]

test_suite(
    name = "warnings_negative_test",
    tests = [":fail_{}_build_error_test".format(case) for case, _ in WARNING_FLAGS_BY_CASE.items()],
)
