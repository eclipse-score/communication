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

load("@trlc//:trlc.bzl", "trlc_requirements", "trlc_requirements_test")

trlc_requirements(
    name = "feature_requirements_ipc",
    srcs = [
        "feature_requirements_ipc.trlc",
        "//score/mw/com/requirements/assumed_system_requirements",
    ],
    spec = [
        "@//third_party/score_requirement_model:requirement_metamodel",
    ],
    visibility = ["//visibility:public"],
)

trlc_requirements_test(
    name = "feature_requirements_ipc_test",
    reqs = [
        ":feature_requirements_ipc",
    ],
    visibility = ["//visibility:public"],
)

# lobster_trlc(
#     name = "feature_requirements_lobster",
#     config = "@//platform/aas/tools/traceability/config/lobster:feature_reqs_conf",
#     requirements = [
#         ":assumed_feature_requirements_ipc",
#     ],
#     visibility = ["//visibility:public"],
# )

# lobster_test(
#     name = "traceability",
#     config = "@//platform/aas/tools/traceability/config/lobster:traceability_feat_conf",
#     inputs = [
#         "@//score/mw/com/requirements/assumed_system_requirements:assumed_system_requirements_lobster",
#         ":feature_requirements_lobster",
#     ],
# )
