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

load("@rules_pkg//pkg:tar.bzl", "pkg_tar")
load("@score_cpp_policies//sanitizers/private:merge_suppressions.bzl", "merge_suppressions")

# Concatenates the per-sanitizer env files since oci_image.env only takes one file.
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
    visibility = ["//visibility:public"],
)

# Avoids pkg_tar silently overwriting same-named suppression files.
merge_suppressions(
    name = "merged_suppressions",
    srcs = [
        "//quality/sanitizer:repo_suppressions",
        "@score_cpp_policies//sanitizers:suppressions",
    ],
)

pkg_tar(
    name = "suppressions_pkg",
    srcs = [":merged_suppressions"],
    mode = "0444",
    package_dir = "sanitizers/suppressions",
    visibility = ["//visibility:public"],
)
