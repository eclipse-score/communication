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
load(
    "@score_tooling//bazel/rules/rules_score:rules_score.bzl",
    "unit",
)

unit(
    name = "impl_skeleton",
    # This path has to be absolute otherwise we get an error when building the dependable element
    scope = ["//score/mw/com/impl_new/binding_independent/skeleton/skeleton:skeleton_lib"],
    tests = [
        ":skeleton_test",
    ],
    unit_design = [
        "//score/mw/com/impl_new/binding_independent/skeleton/skeleton/docs:unit_design",
    ],
    visibility = ["//score/mw/com/impl_new/binding_independent/skeleton:__pkg__"],
    implementation = [
        ":skeleton_lib",
    ],
)

cc_library(
    name = "skeleton_lib",
    srcs = ["skeleton.cpp"],
    hdrs = ["skeleton.h"],
    # visibility = ["//visibility:public"],
    deps = [],
)

cc_test(
    name = "skeleton_test",
    srcs = ["skeleton_test.cpp"],
    # visibility = ["//visibility:public"],
    deps = [
        ":skeleton_lib",
        "@googletest//:gtest_main",
    ],
)
