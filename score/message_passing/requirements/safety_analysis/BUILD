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
    "fmea",
)

trlc_requirements(
    name = "failure_modes",
    srcs = [
        "failure_modes.trlc",
    ],
    spec = [
        "@score_tooling//bazel/rules/rules_score/trlc/config:score_requirements_model",
    ],
    visibility = ["//visibility:public"],
)

trlc_requirements(
    name = "control_measures",
    srcs = [
        "control_measures.trlc",
    ],
    spec = [
        "@score_tooling//bazel/rules/rules_score/trlc/config:score_requirements_model",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":failure_modes",
    ],
)

trlc_requirements_test(
    name = "fmea_test",
    reqs = [
        ":control_measures",
    ],
    visibility = ["//visibility:public"],
)

fmea(
    name = "message_passing_fmea",
    arch_design = "//score/message_passing/design:message_passing_design",
    controlmeasures = [":control_measures"],
    failuremodes = [":failure_modes"],
    root_causes = [],
    visibility = ["//visibility:public"],
)
