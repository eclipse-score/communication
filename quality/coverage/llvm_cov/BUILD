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

load("@rules_python//python:defs.bzl", "py_binary")
load("//quality/coverage/llvm_cov:reporter_wrapper.bzl", "reporter_wrapper")

py_binary(
    name = "merger",
    srcs = ["merger.py"],
)

py_binary(
    name = "reporter",
    testonly = True,
    srcs = ["reporter.py"],
    data = [
        "//:MODULE.bazel",
        "@llvm_toolchain//:llvm-cov",
        "@llvm_toolchain//:llvm-profdata",
        # Demangler for C++ (Itanium) and Rust (v0/legacy) symbol names.
        "@llvm_toolchain_llvm//:bin/llvm-cxxfilt",
    ],
    deps = ["@rules_python//python/runfiles"],
)

reporter_wrapper(
    name = "reporter_wrapper",
    testonly = True,
    coverage_scope = "//quality/coverage:coverage_scope",
    module_bazel = "//:MODULE.bazel",
    reporter = ":reporter",
)
