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

load("@score_tooling//bazel/rules/rules_score:rules_score.bzl", "component", "unit")

component(
    name = "routing_module",
    components = [
        ":event_handler",
        ":routing_manager",
    ],
    design_name = "RoutingModule",
    requirements = [
        "//score/someip/routing_module/requirements/component_requirements:component_requirements",
        "//score/someip/dependability/requirements/feature_requirements:feature_requirements_routing_module",
    ],
    tags = ["manual"],
    target_compatible_with = ["@platforms//os:linux"],
    tests = [],
    visibility = ["//score/someip/dependability:__pkg__"],
)

unit(
    name = "event_handler",
    design_name = "EventHandler",
    maturity = "development",
    scope = [],
    test_specs = [],
    tests = [],
    unit_design = [],
    visibility = ["//score/someip/dependability:__subpackages__"],
    implementation = [],
)

unit(
    name = "routing_manager",
    design_name = "RoutingManager",
    maturity = "development",
    scope = [],
    test_specs = [],
    tests = [],
    unit_design = [],
    visibility = ["//score/someip/dependability:__subpackages__"],
    implementation = [],
)
