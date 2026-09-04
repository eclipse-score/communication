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

# GCC warning features.

load("@rules_cc//cc/toolchains:args.bzl", "cc_args")
load("@rules_cc//cc/toolchains:feature.bzl", "cc_feature")

cc_feature(
    name = "default_flags",
    args = [
        "//quality/compiler_warnings:default_compile_args",
        "//quality/compiler_warnings:default_link_args",
    ],
    feature_name = "score_communication_qcc_default_flags",
    visibility = ["//visibility:public"],
)

alias(
    name = "minimal_warnings",
    actual = "//quality/compiler_warnings/gcc:minimal_warnings",
    visibility = ["//visibility:public"],
)

alias(
    name = "strict_warnings",
    actual = "//quality/compiler_warnings/gcc:strict_warnings",
    visibility = ["//visibility:public"],
)

alias(
    name = "strict_warnings_no_error",
    actual = "//quality/compiler_warnings/gcc:strict_warnings_no_error",
    visibility = ["//visibility:public"],
)

alias(
    name = "additional_warnings",
    actual = "//quality/compiler_warnings/gcc:additional_warnings",
    visibility = ["//visibility:public"],
)

alias(
    name = "third_party_warnings",
    actual = "//quality/compiler_warnings/gcc:third_party_warnings",
    visibility = ["//visibility:public"],
)
