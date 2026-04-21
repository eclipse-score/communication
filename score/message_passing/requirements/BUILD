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

load("@trlc//:trlc.bzl", "trlc_requirements", "trlc_requirements_test")
load(
    "@score_tooling//bazel/rules/rules_score:rules_score.bzl",
    "assumptions_of_use",
    "component",
    "component_requirements",
    "dependability_analysis",
    "dependable_element",
    "feature_requirements",
)

trlc_requirements(
    name = "aous_trlc",
    srcs = [
        "aous.trlc",
    ],
    spec = [
        "@score_tooling//bazel/rules/rules_score/trlc/config:score_requirements_model",
    ],
    visibility = ["//visibility:public"],
)

trlc_requirements_test(
    name = "aous_test",
    reqs = [
        ":aous_trlc",
    ],
    visibility = ["//visibility:public"],
)

assumptions_of_use(
    name = "aous",
    srcs = ["aous.trlc"],
    visibility = ["//visibility:public"],
)

trlc_requirements(
    name = "assumed_system_requirements",
    srcs = [
        "assumed_system_requirements.trlc",
    ],
    spec = [
        "@score_tooling//bazel/rules/rules_score/trlc/config:score_requirements_model",
    ],
    visibility = ["//visibility:public"],
)

trlc_requirements_test(
    name = "assumed_system_requirements_test",
    reqs = [
        ":assumed_system_requirements",
    ],
    visibility = ["//visibility:public"],
)

trlc_requirements(
    name = "feature_requirements_trlc",
    srcs = [
        "feature_requirements.trlc",
    ],
    spec = [
        "@score_tooling//bazel/rules/rules_score/trlc/config:score_requirements_model",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":assumed_system_requirements",
    ],
)

feature_requirements(
    name = "feature_requirements",
    srcs = [":feature_requirements_trlc"],
    visibility = ["//visibility:public"],
)

trlc_requirements(
    name = "component_requirements_trlc",
    srcs = [
        "component_requirements.trlc",
    ],
    spec = [
        "@score_tooling//bazel/rules/rules_score/trlc/config:score_requirements_model",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":assumed_system_requirements",
        ":feature_requirements_trlc",
    ],
)

component_requirements(
    name = "component_requirements",
    srcs = [":component_requirements_trlc"],
    visibility = ["//visibility:public"],
)

component(
    name = "component_message_passing",
    components = [
        "//score/message_passing/requirements/server:server",
        "//score/message_passing/requirements/client:client",
    ],
    requirements = [
        ":component_requirements",
    ],
    tests = [],
)

dependability_analysis(
    name = "message_passing_dependability_analysis",
    arch_design = "//score/message_passing/design:message_passing_design",
    fmea = ["//score/message_passing/requirements/safety_analysis:message_passing_fmea"],
)

dependable_element(
    name = "dependable_element_message_passing",
    architectural_design = ["//score/message_passing/design:message_passing_design"],
    assumptions_of_use = [":aous"],
    components = [":component_message_passing"],
    dependability_analysis = [":message_passing_dependability_analysis"],
    description = "Message Passing - inter-process communication component providing client-server mechanisms",
    integrity_level = "B",
    requirements = [
        ":feature_requirements",
    ],
    tests = [],
)
