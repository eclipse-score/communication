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
    "assumed_system_requirements",
    "assumptions_of_use",
)
load("@trlc//:trlc.bzl", "trlc_requirements")

trlc_requirements(
    name = "aous_trlc",
    srcs = [
        "aous.trlc",
    ],
    spec = [
        "@score_tooling//bazel/rules/rules_score/trlc/config:score_requirements_model",
    ],
    target_compatible_with = ["@platforms//os:linux"],
    visibility = ["//score/message_passing/dependability:__subpackages__"],
)

assumptions_of_use(
    name = "aous",
    srcs = [":aous_trlc"],
    target_compatible_with = ["@platforms//os:linux"],
    visibility = ["//score/message_passing/dependability:__subpackages__"],
)

assumed_system_requirements(
    name = "assumed_system_requirements",
    srcs = ["assumed_system_requirements.trlc"],
    target_compatible_with = ["@platforms//os:linux"],
    visibility = ["//score/message_passing/dependability:__subpackages__"],
)
