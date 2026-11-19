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
    "feature_requirements",
)

feature_requirements(
    name = "feature_requirements_routing_module",
    srcs = ["feature_requirements_routing_module.trlc"],
    target_compatible_with = ["@platforms//os:linux"],
    visibility = [
        "//score/someip/dependability:__subpackages__",
        "//score/someip/routing_module:__subpackages__",
    ],
    deps = ["//score/dependability/assumed_system:assumed_system_requirements"],
)

feature_requirements(
    name = "feature_requirements_sd_daemon",
    srcs = ["feature_requirements_sd_daemon.trlc"],
    target_compatible_with = ["@platforms//os:linux"],
    visibility = [
        "//score/someip/dependability:__subpackages__",
        "//score/someip/sd_daemon:__subpackages__",
    ],
    deps = ["//score/dependability/assumed_system:assumed_system_requirements"],
)
