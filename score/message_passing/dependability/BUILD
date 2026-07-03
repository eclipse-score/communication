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
    "component",
    "dependable_element",
)

component(
    name = "component_message_passing",
    components = [
        "//score/message_passing:client_connection",
        "//score/message_passing:server_connection",
        ":dispatch",
    ],
    requirements = [
        "//score/message_passing/dependability/requirements:component_requirements",
    ],
    tags = ["manual"],
    target_compatible_with = ["@platforms//os:linux"],
    tests = [],
)

component(
    name = "dispatch",
    components = [
        "//score/message_passing:unix_domain",
        "//score/message_passing:qnx_dispatch",
    ],
    requirements = [
        "//score/message_passing/dependability/requirements:component_requirements",
    ],
    tags = ["manual"],
    target_compatible_with = ["@platforms//os:linux"],
    tests = [],
)

dependable_element(
    name = "dependable_element_message_passing",
    architectural_design = ["//score/message_passing/dependability/software_architectural_design:message_passing_architectural_design"],
    assumptions_of_use = ["//score/message_passing/dependability/assumed_system:aous"],
    components = [":component_message_passing"],
    dependability_analysis = ["//score/message_passing/dependability/safety_analysis:message_passing_dependability_analysis"],
    integrity_level = "B",
    maturity = "development",
    requirements = [
        "//score/message_passing/dependability/requirements:feature_requirements",
        "//score/message_passing/dependability/assumed_system:assumed_system_requirements",
    ],
    tags = ["manual"],
    target_compatible_with = ["@platforms//os:linux"],
    tests = [
        "//score/message_passing:unit_tests",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//third_party/score_baselibs:bitmanipulation",
        "//third_party/score_baselibs:containers",
        "//third_party/score_baselibs:futurecpp",
        "//third_party/score_baselibs:os",
    ],
)
