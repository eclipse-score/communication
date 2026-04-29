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
    "assumed_system_requirements",
    "assumptions_of_use",
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

assumptions_of_use(
    name = "aous",
    srcs = [":aous_trlc"],
    visibility = ["//visibility:public"],
)

trlc_requirements(
    name = "assumed_system_requirements_trlc",
    srcs = [
        "assumed_system_requirements.trlc",
    ],
    spec = [
        "@score_tooling//bazel/rules/rules_score/trlc/config:score_requirements_model",
    ],
    visibility = ["//visibility:public"],
)

trlc_requirements_test(
    name = "assumed_system_requirements_trlc_test",
    reqs = [":assumed_system_requirements_trlc"],
    visibility = ["//visibility:public"],
)

assumed_system_requirements(
    name = "assumed_system_requirements",
    srcs = [":assumed_system_requirements_trlc"],
    visibility = ["//visibility:public"],
)
