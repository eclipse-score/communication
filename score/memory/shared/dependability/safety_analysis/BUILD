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
    "assumptions_of_use",
    "dependability_analysis",
    "failure_modes",
    "fault_trees",
    "safety_analysis",
)

filegroup(
    name = "fta_files",
    srcs = [
        "fta_object_of_unexpected_creator_provided.puml",
    ],
    visibility = ["//score/memory/shared/dependability:__pkg__"],
)

failure_modes(
    name = "memory_shared_failure_modes",
    srcs = ["failure_modes.trlc"],
    target_compatible_with = ["@platforms//os:linux"],
    deps = ["//score/memory/shared/dependability/architectural_design:software_architectural_design"],
)

fault_trees(
    name = "memory_shared_fault_trees",
    srcs = [":fta_files"],
    fta_package = "memory_shared_fta",
    target_compatible_with = ["@platforms//os:linux"],
    deps = [":memory_shared_failure_modes"],
)

assumptions_of_use(
    name = "memory_shared_aous",
    srcs = ["aous.trlc"],
    target_compatible_with = ["@platforms//os:linux"],
)

safety_analysis(
    name = "memory_shared_safety_analysis",
    assessments = ["assessments.trlc"],
    failure_modes = [":memory_shared_failure_modes"],
    fault_trees = ":memory_shared_fault_trees",
    safety_measures = [
        ":memory_shared_aous",
        "//score/memory/shared/dependability/requirements:component_requirements",
    ],
    visibility = ["//score/memory/shared/dependability:__pkg__"],
)

dependability_analysis(
    name = "memory_shared_dependability_analysis",
    arch_design = "//score/memory/shared/dependability/architectural_design:software_architectural_design",
    safety_analysis = [":memory_shared_safety_analysis"],
    tags = ["manual"],
    visibility = ["//score/memory/shared/dependability:__pkg__"],
)
