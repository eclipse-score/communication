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
load("@score_coverage//:defs.bzl", "score_coverage_reporter", "score_coverage_scope")

# Coverage scope: the dependable elements of the module. The aspect walks the
# component / unit hierarchy into the implementation libraries and their
# transitive in-workspace deps; everything else (tests, mocks, external
# dependencies) is filtered out of the report. A production library missing
# here silently vanishes from the report, so keep this list complete.
score_coverage_scope(
    name = "coverage_scope",
    testonly = True,
    visibility = ["//quality/coverage:__subpackages__"],
    deps = [
        "//score/message_passing",
        "//score/mw/com",
        "//score/mw/com/rust:score_com",
    ],
)

# Linux: LLVM source-based coverage (C++ and Rust). Selected by the default
# coverage config in coverage.bazelrc.
score_coverage_reporter(
    name = "reporter_wrapper",
    testonly = True,
    coverage_scope = ":coverage_scope",
    llvm_cov = "@llvm_toolchain//:llvm-cov",
    llvm_cxxfilt = "@llvm_toolchain_llvm//:bin/llvm-cxxfilt",
    llvm_profdata = "@llvm_toolchain//:llvm-profdata",
)

# QNX on target: QCC is GCC-based, so the counters are gcov's. Selected by
# --config=qnx under bazel coverage. C++ only. Manual: the target depends on
# the QNX SDP package, which a wildcard build on a Linux host without QNX
# credentials must not try to fetch.
score_coverage_reporter(
    name = "gcov_reporter_wrapper",
    testonly = True,
    backend = "gcov",
    coverage_scope = ":coverage_scope",
    gcov = "@score_qcc_x86_64_toolchain_pkg//:gcov",
    tags = ["manual"],
)
