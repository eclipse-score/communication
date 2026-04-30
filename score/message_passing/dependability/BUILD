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
    "dependability_analysis",
    "dependable_element",
    "feature_requirements",
)

feature_requirements(
    name = "feature_requirements",
    srcs = ["//score/message_passing/dependability/requirements:feature_requirements_trlc"],
    visibility = ["//visibility:public"],
)

dependability_analysis(
    name = "message_passing_dependability_analysis",
    arch_design = "//score/message_passing/dependability/software_architectural_design:message_passing_architectural_design",
    fmea = ["//score/message_passing/dependability/safety_analysis:message_passing_fmea"],
    tags = ["manual"],
)

dependable_element(
    name = "dependable_element_message_passing",
    architectural_design = ["//score/message_passing/dependability/software_architectural_design:message_passing_architectural_design"],
    assumptions_of_use = ["//score/message_passing/dependability/assumed_system:aous"],
    components = ["//score/message_passing/dependability/units:component_message_passing"],
    dependability_analysis = [":message_passing_dependability_analysis"],
    integrity_level = "B",
    maturity = "development",
    requirements = [
        ":feature_requirements",
    ],
    tests = [
        "//score/message_passing:unit_tests",
    ],
)
