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
    name = "someip",
    architectural_design = ["//score/someip/dependability/architectural_design:software_architectural_design"],
    components = [
        "//score/someip/sd_daemon:sd_daemon",
        "//score/someip/routing_module:routing_module",
        "//score/someip:router_control_protocol",
        "//score/someip:sd_control_protocol",
    ],
    dependability_analysis = [],
    integrity_level = "QM",
    maturity = "development",
    requirements = [
        "//score/dependability/assumed_system:assumed_system_requirements",
    ],
    target_compatible_with = ["@platforms//os:linux"],
    tests = [],
    visibility = ["//visibility:public"],
)
