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
load("@score_tooling//bazel/rules/rules_score:rules_score.bzl", "dependable_element")

dependable_element(
    name = "mw_com",
    architectural_design = ["//score/mw/com/dependability/software_architectural_design"],
    assumptions_of_use = [],
    components = [
        "//score/mw/com/impl:frontend_component",
        "//score/mw/com/impl/bindings",
    ],
    dependability_analysis = ["//score/mw/com/dependability/safety_analysis:mw_com_fmea"],
    integrity_level = "B",
    maturity = "development",
    requirements = [],
    target_compatible_with = ["@platforms//os:linux"],
    tests = [],
)
