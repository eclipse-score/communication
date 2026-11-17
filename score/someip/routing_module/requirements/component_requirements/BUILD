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
    "component_requirements",
)

component_requirements(
    name = "component_requirements",
    srcs = ["component_requirements.trlc"],
    target_compatible_with = ["@platforms//os:linux"],
    visibility = ["//score/someip/routing_module:__pkg__"],
    deps = [
        "//score/someip/dependability/requirements/feature_requirements:feature_requirements_routing_module",
    ],
)
