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
    name = "component_service_discovery",
    components = [
        "//score/mw/com/service_discovery:service_discovery_daemon_unit",
        "//score/mw/com/service_discovery:service_discovery_message_passing_client_unit",
        "//score/mw/com/service_discovery:service_discovery_message_passing_server_unit",
        "//score/mw/com/service_discovery:service_discovery_protocol_unit",
        "//score/mw/com/service_discovery:service_discovery_registry_unit",
    ],
    requirements = ["//score/mw/com/service_discovery/dependability/requirements:component_requirements"],
    tags = ["manual"],
    target_compatible_with = ["@platforms//os:linux"],
    tests = [],
)

dependable_element(
    name = "dependable_element_service_discovery",
    architectural_design = ["//score/mw/com/service_discovery/dependability/software_architectural_design:service_discovery_architectural_design"],
    assumptions_of_use = ["//score/mw/com/service_discovery/dependability/assumed_system:aous"],
    components = [":component_service_discovery"],
    dependability_analysis = ["//score/mw/com/service_discovery/dependability/safety_analysis:service_discovery_dependability_analysis"],
    integrity_level = "B",
    maturity = "development",
    requirements = [
        "//score/mw/com/service_discovery/dependability/requirements:feature_requirements",
        "//score/mw/com/service_discovery/dependability/assumed_system:assumed_system_requirements",
    ],
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
