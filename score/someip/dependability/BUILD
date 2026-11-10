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
load("@score_tooling//bazel/rules/rules_score:rules_score.bzl", "component", "dependable_element")

component(
    name = "component_sd_daemon",
    components = [
    ],
    requirements = [
        "//score/someip/dependability/requirements/sd_daemon/component_requirements",
        "//score/someip/dependability/requirements/sd_daemon/feature_requirements",
    ],
    tags = ["manual"],
    target_compatible_with = ["@platforms//os:linux"],
    tests = [],
)

component(
    name = "component_routing_module",
    components = [
    ],
    requirements = [
        "//score/someip/dependability/requirements/routing_module/component_requirements",
        "//score/someip/dependability/requirements/routing_module/feature_requirements",
    ],
    tags = ["manual"],
    target_compatible_with = ["@platforms//os:linux"],
    tests = [],
)

dependable_element(
    name = "someip",
    architectural_design = [],
    components = [
        ":component_sd_daemon",
        ":component_routing_module",
    ],
    dependability_analysis = [],
    integrity_level = "QM",
    maturity = "development",
    requirements = [
        "//score/mw/com/dependability/assumed_system:assumed_system_requirements",
    ],
    target_compatible_with = ["@platforms//os:linux"],
    tests = [],
    visibility = ["//visibility:public"],
)
