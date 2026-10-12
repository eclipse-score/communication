# *******************************************************************************
# Copyright (c) 2024 Contributors to the Eclipse Foundation
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
    "safety_analysis",
)

# Root Causes
filegroup(
    name = "root_causes",
    srcs = [
        "//score/mw/com/dependability/safety_analysis/root_causes:general",
        "//score/mw/com/dependability/safety_analysis/root_causes/proxy:root_causes_proxy",
        "//score/mw/com/dependability/safety_analysis/root_causes/skeleton:root_causes_skeleton",
        "//score/mw/com/dependability/safety_analysis/root_causes/smart_pointer:smart_pointer_fta",
    ],
    visibility = ["//score/mw/com/dependability:__subpackages__"],
)

safety_analysis(
    name = "mw_com_safety_analysis",
    arch_design = "//score/mw/com/dependability/architectural_design:software_architectural_design",
    failuremodes = ["failure_modes.trlc"],
    fta_package = "mw_com_fta",
    root_causes = [":root_causes"],
    safetymeasures = [
        "aou.trlc",
        "safetymeasures.trlc",
    ],
    visibility = ["//score/mw/com/dependability:__subpackages__"],
)

assumptions_of_use(
    name = "aous",
    srcs = ["aou.trlc"],
    target_compatible_with = ["@platforms//os:linux"],
    visibility = ["//score/mw/com/dependability:__subpackages__"],
    # Resolves mw_com_fta.<RootCause> in AoU.root_causes.
    deps = [":mw_com_safety_analysis"],
)

# dependable_element consumes DependabilityAnalysisInfo, which only this rule
# provides -- passing the safety_analysis target directly drops it from traceability.
# maturity="development": traceability chain (public_api wiring, AoU root-cause
# links) is not complete yet -- see communication#1075 for the migration plan.
dependability_analysis(
    name = "mw_com_dependability_analysis",
    arch_design = "//score/mw/com/dependability/architectural_design:software_architectural_design",
    maturity = "development",
    safety_analysis = [":mw_com_safety_analysis"],
    visibility = ["//score/mw/com/dependability:__subpackages__"],
)
