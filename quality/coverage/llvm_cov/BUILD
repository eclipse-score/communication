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

load(
    "@score_cpp_policies//coverage:defs.bzl",
    "score_coverage_reporter",
    "score_instrumented_sources_manifest",
)

# Collect all C++ source files reachable from the main library targets so that
# files not linked into any test show up at 0% in the coverage report.
score_instrumented_sources_manifest(
    name = "instrumented_sources",
    targets = [
        "//score/message_passing:message_passing",
        "//score/mw/com:com",
    ],
)

# Wrapper target wiring in the LLVM toolchain binaries and the workspace root.
# Referenced by coverage.bazelrc via:
#   coverage --coverage_report_generator=//quality/coverage/llvm_cov:reporter_wrapper
score_coverage_reporter(
    name = "reporter_wrapper",
    instrumented_sources_manifest = ":instrumented_sources",
    llvm_cov = "@llvm_toolchain//:llvm-cov",
    llvm_profdata = "@llvm_toolchain//:llvm-profdata",
    visibility = ["//visibility:public"],
)
