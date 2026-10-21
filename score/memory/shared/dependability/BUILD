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
    "dependable_element",
    "unit_design",
)

# Class diagrams are passed through directly to Sphinx (sphinxcontrib-plantuml
# renders them at doc-build time). The RST wrapper references the .puml file
# via '.. uml::' so both must be listed together in 'static'.
unit_design(
    name = "unit_design",
    static = ["public_api.puml"],
)

# Step 0 (tutorial) setup: empty target: architectural_design, requirements,
# assumptions_of_use, and dependability_analysis are filled in by later stories.
dependable_element(
    name = "dependable_element_memory_shared",
    architectural_design = [],
    assumptions_of_use = [],
    components = [],
    dependability_analysis = [],
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
