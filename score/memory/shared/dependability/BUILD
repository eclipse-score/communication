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
    "@score_tooling//bazel/rules/rules_score:rules_score.bzl",
    "component",
    "dependable_element",
)

exports_files(
    ["public_api.puml"],
    visibility = ["//score/memory/shared/dependability/architectural_design:__pkg__"],
)

component(
    name = "memory_shared",
    components = [
        "//score/memory/shared:allocator_pointer",
        "//score/memory/shared:factory_resources",
        "//score/memory/shared:pointer_arithmetic",
    ],
    tags = ["manual"],
    target_compatible_with = ["@platforms//os:linux"],
    tests = [],
)

# Step 0 (tutorial) setup: requirements and assumptions_of_use are filled in
# by later stories.
dependable_element(
    name = "dependable_element_memory_shared",
    architectural_design = ["//score/memory/shared/dependability/architectural_design:software_architectural_design"],
    components = [":memory_shared"],
    dependability_analysis = ["//score/memory/shared/dependability/safety_analysis:memory_shared_dependability_analysis"],
    integrity_level = "B",
    maturity = "development",
    requirements = [],
    tags = ["manual"],
    target_compatible_with = ["@platforms//os:linux"],
    tests = [],
    visibility = ["//visibility:public"],
    deps = [
        "//third_party/score_baselibs:bitmanipulation",
        "//third_party/score_baselibs:containers",
        "//third_party/score_baselibs:futurecpp",
        "//third_party/score_baselibs:os",
    ],
)
