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

load("@rules_pkg//pkg:tar.bzl", "pkg_tar")

filegroup(
    name = "repo_suppressions",
    srcs = [
        "asan.supp",
        "lsan.supp",
        "tsan.supp",
        "ubsan.supp",
    ],
)

# FIXME: remove once score_cpp_policies provides a pre-merged env target — https://github.com/eclipse-score/score_cpp_policies/issues/30
genrule(
    name = "merged_absolute_env",
    srcs = select({
        "@score_cpp_policies//sanitizers/flags:asan_on": ["@score_cpp_policies//sanitizers:asan_absolute_env"],
        "//conditions:default": [],
    }) + select({
        "@score_cpp_policies//sanitizers/flags:ubsan_on": ["@score_cpp_policies//sanitizers:ubsan_absolute_env"],
        "//conditions:default": [],
    }) + select({
        "@score_cpp_policies//sanitizers/flags:lsan_on": ["@score_cpp_policies//sanitizers:lsan_absolute_env"],
        "//conditions:default": [],
    }) + select({
        "@score_cpp_policies//sanitizers/flags:tsan_on": ["@score_cpp_policies//sanitizers:tsan_absolute_env"],
        "//conditions:default": [],
    }),
    outs = ["merged_absolute_sanitizer.env"],
    cmd = "cat $(SRCS) > $@",
    target_compatible_with = ["@score_cpp_policies//sanitizers/constraints:any_sanitizer"],
    visibility = ["//:__subpackages__"],
)

pkg_tar(
    name = "suppressions_pkg",
    srcs = [":repo_suppressions"],
    mode = "0444",
    package_dir = "sanitizers/suppressions",
    visibility = ["//:__subpackages__"],
)
