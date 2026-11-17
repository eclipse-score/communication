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
    name = "sd_daemon",
    components = [
        ":sd_facade",
        ":sd_only_router",
        ":service_manager",
    ],
    design_name = "SDDaemon",
    requirements = [
        "//score/someip/sd_daemon/requirements/component_requirements:component_requirements",
        "//score/someip/dependability/requirements/feature_requirements:feature_requirements_sd_daemon",
    ],
    tags = ["manual"],
    target_compatible_with = ["@platforms//os:linux"],
    tests = [],
    visibility = ["//score/someip/dependability:__pkg__"],
)

unit(
    name = "sd_facade",
    design_name = "SDFacade",
    maturity = "development",
    scope = [],
    test_specs = [],
    tests = [],
    unit_design = [],
    visibility = ["//score/someip/dependability:__subpackages__"],
    implementation = [],
)

unit(
    name = "service_manager",
    design_name = "ServiceManager",
    maturity = "development",
    scope = [],
    test_specs = [],
    tests = [],
    unit_design = [],
    visibility = ["//score/someip/dependability:__subpackages__"],
    implementation = [],
)

unit(
    name = "sd_only_router",
    design_name = "SDOnlyRouter",
    maturity = "development",
    scope = [],
    test_specs = [],
    tests = [],
    unit_design = [],
    visibility = ["//score/someip/dependability:__subpackages__"],
    implementation = [],
)
