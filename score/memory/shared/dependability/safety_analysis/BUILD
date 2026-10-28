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
    "dependability_analysis",
    "failure_modes",
    "fault_trees",
    "safety_analysis",
)

# No FailureMode records yet: wiring this empty source still activates the
# Public API coverage check, so every public_api interface element is
# reported as missing a failure-mode link.
failure_modes(
    name = "memory_shared_failure_modes",
    srcs = ["failure_modes.trlc"],
    target_compatible_with = ["@platforms//os:linux"],
)

fault_trees(
    name = "memory_shared_fault_trees",
    target_compatible_with = ["@platforms//os:linux"],
    deps = [":memory_shared_failure_modes"],
)

safety_analysis(
    name = "memory_shared_safety_analysis",
    arch_design = "//score/memory/shared/dependability:memory_shared_architectural_design",
    failure_modes = [":memory_shared_failure_modes"],
    fault_trees = ":memory_shared_fault_trees",
    visibility = ["//score/memory/shared/dependability:__pkg__"],
)

dependability_analysis(
    name = "memory_shared_dependability_analysis",
    arch_design = "//score/memory/shared/dependability:memory_shared_architectural_design",
    safety_analysis = [":memory_shared_safety_analysis"],
    tags = ["manual"],
    visibility = ["//score/memory/shared/dependability:__pkg__"],
)
