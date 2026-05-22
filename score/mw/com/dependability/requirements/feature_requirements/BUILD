# *******************************************************************************
# Copyright (c) 2024 Contributors to the Eclipse Foundation
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
load(
    "@score_tooling//validation/ai_checker:ai_checker.bzl",
    "trlc_requirements_ai_test",
)

feature_requirements(
    name = "feature_requirements_ipc",
    srcs = ["feature_requirements_ipc.trlc"],
    target_compatible_with = ["@platforms//os:linux"],
    visibility = ["//visibility:public"],
    deps = ["//score/mw/com/dependability/requirements/assumed_system_requirements"],
)

trlc_requirements_ai_test(
    name = "requirements_ai_check",
    batch_size = 10,
    reqs = [":feature_requirements_ipc"],
    tags = ["manual"],
)
