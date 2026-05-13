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
        ":client",
        ":server",
    ],
    requirements = [
        "//score/message_passing/dependability/requirements:component_requirements",
    ],
    tags = ["manual"],
    target_compatible_with = ["@platforms//os:linux"],
    tests = [],
)

# Component: client
# Covers the client-side connection management and common messaging infrastructure
component(
    name = "client",
    components = [
        "//score/message_passing:client_connection",
    ],
    requirements = [
        "//score/message_passing/dependability/requirements:component_requirements",
    ],
    tags = ["manual"],
    target_compatible_with = ["@platforms//os:linux"],
    tests = [],
)

# Component: server
# Covers the platform-specific server implementations (QNX dispatch and Unix domain)
component(
    name = "server",
    components = select({
        "@platforms//os:qnx": [
            "//score/message_passing:qnx_dispatch",
            "//score/message_passing:unix_domain",
        ],
        "//conditions:default": ["//score/message_passing:unix_domain"],
    }),
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
)
