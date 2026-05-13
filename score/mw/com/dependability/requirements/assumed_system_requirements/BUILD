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

load(
    "@score_tooling//bazel/rules/rules_score:rules_score.bzl",
    "assumed_system_requirements",
)

assumed_system_requirements(
    name = "assumed_system_requirements",
    srcs = ["assumed_system_requirements.trlc"],
    target_compatible_with = ["@platforms//os:linux"],
    visibility = ["//visibility:public"],
)
