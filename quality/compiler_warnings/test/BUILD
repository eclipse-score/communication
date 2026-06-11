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
load("//score/mw:common_features.bzl", "COMPILER_WARNING_FEATURES")

# ==============================================================================
# Compiler Warning Feature Tests
#
# Test strategy:
#   1. Enabled feature + no bug → no error (fp_all / fp_build_test)
#   2. Bug + disabled feature → no error (tp_all / tp_build_test)
#   3. (Future ticket) Bug + enabled feature → error
# ==============================================================================

# Clean code: compiles with all warning features enabled.
# Verifies that correct code is not rejected by the warning features.
cc_library(
    name = "fp_all",
    srcs = ["clean.cpp"],
    features = COMPILER_WARNING_FEATURES,
)

# Verifiable via `bazel test` — asserts fp_all compiles without error.
build_test(
    name = "fp_build_test",
    targets = [":fp_all"],
)

# Faulty code: triggers warnings but features are disabled.
# Verifies that disabling features allows code with warnings to compile.
cc_library(
    name = "tp_all",
    srcs = ["has_warnings.cpp"],
    features = ["-score_communication_treat_warnings_as_errors"],
    tags = ["manual"],
)

# Verifiable via `bazel test` — asserts tp_all compiles without error
# when warning features are disabled.
build_test(
    name = "tp_build_test",
    targets = [":tp_all"],
)
