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

load("@lobster//:lobster.bzl", "lobster_trlc")
load("@trlc//:trlc.bzl", "trlc_requirements", "trlc_requirements_test")

# Failures Modes
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

trlc_requirements_test(
    name = "failure_modes_test",
    reqs = [
        ":failure_modes",
    ],
    target_compatible_with = ["@platforms//os:linux"],
    visibility = ["//visibility:public"],
)

# Root Causes
filegroup(
    name = "root_causes",
    srcs = [
        "//score/mw/com/dependability/safety_analysis/root_causes:general",
        "//score/mw/com/dependability/safety_analysis/root_causes/proxy:root_causes_proxy",
        "//score/mw/com/dependability/safety_analysis/root_causes/skeleton:root_causes_skeleton",
        "//score/mw/com/dependability/safety_analysis/root_causes/smart_pointer:smart_pointer_fta",
    ],
    visibility = ["//visibility:public"],
)

# Control Measures
# AoUs
trlc_requirements(
    name = "control_measures",
    srcs = [
        "aou.trlc",
    ],
    spec = [
        "@score_tooling//bazel/rules/rules_score/trlc/config:score_requirements_model",
    ],
    visibility = ["//visibility:public"],
)

trlc_requirements_test(
    name = "control_measures_test",
    reqs = [
        ":control_measures",
    ],
    target_compatible_with = ["@platforms//os:linux"],
    visibility = ["//visibility:public"],
)
