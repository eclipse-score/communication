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
    "assumptions_of_use",
)

# TODO: list as safety measure of a someip safety_analysis to make it an AoU of the dependable element.
assumptions_of_use(
    name = "assumptions_of_use",
    srcs = ["assumptions_of_use.trlc"],
    target_compatible_with = ["@platforms//os:linux"],
    visibility = ["//score/someip:__subpackages__"],
)
