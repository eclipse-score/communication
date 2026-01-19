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

load("@aspect_rules_lint//lint:clang_tidy.bzl", "lint_clang_tidy_aspect")
load("@aspect_rules_lint//lint:lint_test.bzl", "lint_test")

# Define the clang-tidy linter aspect using LLVM toolchain
clang_tidy = lint_clang_tidy_aspect(
    binary = Label("@llvm_toolchain//:clang-tidy"),
    configs = [
        Label("//:.clang-tidy"),
    ],
    lint_target_headers = True,
    angle_includes_are_system = True,
    verbose = False,
)

# Create a test rule for clang-tidy (for individual targets)
clang_tidy_test = lint_test(aspect = clang_tidy)

# Export the aspect for use in multirun
clang_tidy_aspect = clang_tidy
