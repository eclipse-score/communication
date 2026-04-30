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
        "//score/message_passing/dependability/assumed_system:assumed_system_requirements_trlc",
    ],
)

trlc_requirements_test(
    name = "feature_requirements_trlc_test",
    reqs = [
        ":feature_requirements_trlc",
    ],
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
        "//score/message_passing/dependability/assumed_system:assumed_system_requirements_trlc",
        ":feature_requirements_trlc",
    ],
)

trlc_requirements_test(
    name = "component_requirements_trlc_test",
    reqs = [
        ":component_requirements_trlc",
    ],
    visibility = ["//visibility:public"],
)
