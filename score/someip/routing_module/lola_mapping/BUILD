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

load("@rules_cc//cc:defs.bzl", "cc_library")
load("@score_tooling//bazel/rules/rules_score:rules_score.bzl", "unit")
load("//quality/unit_testing:unit_testing.bzl", "cc_unit_test")

cc_library(
    name = "lola_mapping",
    srcs = ["lola_mapping.cpp"],
    hdrs = ["lola_mapping.h"],
    visibility = ["//visibility:public"],
)

cc_unit_test(
    name = "lola_mapping_test",
    srcs = ["lola_mapping_test.cpp"],
    deps = [":lola_mapping"],
)

unit(
    name = "lola_mapping_unit",
    design_name = "LolaMapping",
    maturity = "development",
    scope = ["//score/someip/routing_module/lola_mapping:__subpackages__"],
    test_specs = [],
    tests = [":lola_mapping_test_linux"],
    unit_design = ["//score/someip/routing_module/lola_mapping/dependability:lola_mapping_unit_design"],
    visibility = [
        "//score/someip/dependability:__subpackages__",
        "//score/someip/routing_module:__pkg__",
    ],
    implementation = [":lola_mapping"],
)
