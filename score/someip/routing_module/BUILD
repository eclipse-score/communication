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

load("@score_tooling//bazel/rules/rules_score:rules_score.bzl", "component")

component(
    name = "routing_module",
    components = [
        "//score/someip/routing_module/iosock_bridge:iosock_bridge_unit",
        "//score/someip/routing_module/notification_handler:notification_handler_unit",
        "//score/someip/routing_module/iosock_transport:iosock_transport",
        "//score/someip/routing_module/lola_mapping:lola_mapping_unit",
        "//score/someip/routing_module/routing_manager:routing_manager_unit",
    ],
    design_name = "RoutingModule",
    requirements = [
        "//score/someip/routing_module/requirements/component_requirements:component_requirements",
        "//score/someip/dependability/requirements/feature_requirements:feature_requirements_routing_module",
    ],
    tags = ["manual"],
    target_compatible_with = ["@platforms//os:linux"],
    test_case_coverage_lock = "test_case_coverage.lock.yaml",
    tests = [],
    visibility = ["//score/someip/dependability:__pkg__"],
)
