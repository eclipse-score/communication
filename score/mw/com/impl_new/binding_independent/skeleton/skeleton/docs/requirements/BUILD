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

load("@score_tooling//bazel/rules/rules_score:rules_score.bzl", "component_requirements")
load("@score_tooling//validation/ai_checker:ai_checker.bzl", "trlc_requirements_ai_test")

component_requirements(
    name = "component_requirements",
    srcs = ["component_requirements.trlc"],
    visibility = ["//score/mw/com/impl_new/binding_independent/skeleton:__subpackages__"],
    deps = [
        "//score/mw/com/dependability/requirements:feature_requirements",
    ],
)

trlc_requirements_ai_test(
    name = "component_requirements_ai_test",
    reqs = [":component_requirements"],
    tags = ["manual"],
)
