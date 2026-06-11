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

load("@bazel_skylib//rules:build_test.bzl", "build_test")
load("@rules_build_error//lang/cc:defs.bzl", "cc_build_error_test")
load("@rules_build_error//matcher:defs.bzl", "matcher")
load("//score/mw:common_features.bzl", "COMPILER_WARNING_FEATURES")

# ==============================================================================
# Compiler Warning Feature Tests
#
# Test strategy:
#   1. Bug + disabled feature  → no error (tp_<flag> / tp_build_test)
#   2. Enabled feature + no bug → no error (fp_all / fp_build_test)
#   3. Bug + enabled feature    → error    (warnings_negative_test)
#
# Each warning flag has its own source file and its own bazel target so that
# every flag is exercised independently. A single combined target would only
# surface the first flag that triggers, hiding the rest.
# ==============================================================================

# Uses rules_build_error to assert compilation fails directly.
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

# Test 1: faulty code triggers a warning but treat_warnings_as_errors is disabled.
# One target per flag — verifies that disabling the feature allows code with that
# specific warning to compile.
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

# Verifiable via `bazel test` — asserts each tp_<flag> compiles without error
# when treat_warnings_as_errors is disabled.
build_test(
    name = "tp_build_test",
    targets = [":tp_{}".format(case) for case, _ in WARNING_FLAGS_BY_CASE.items()],
)

# Test 2: clean code compiles with all warning features enabled.
# Verifies that correct code is not rejected by the warning features.
cc_library(
    name = "fp_all",
    srcs = ["no_warnings/clean.cpp"],
    features = COMPILER_WARNING_FEATURES,
)

# Verifiable via `bazel test` — asserts fp_all compiles without error.
build_test(
    name = "fp_build_test",
    targets = [":fp_all"],
)

# Test 3: bug + enabled feature must fail the build.

[
    cc_build_error_test(
        name = "fail_{}_build_error_test".format(case),
        src = "has_warnings/{}.cpp".format(case),
        compile_stderr = matcher.has_substr("error:"),
        copts = ["-Werror"] + warning,
    )
    for case, warning in WARNING_FLAGS_BY_CASE.items()
]

test_suite(
    name = "warnings_negative_test",
    tests = [":fail_{}_build_error_test".format(case) for case, _ in WARNING_FLAGS_BY_CASE.items()],
)
