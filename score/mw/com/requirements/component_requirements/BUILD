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
    name = "component_requirements_ipc",
    srcs = [
        "component_requirements_ipc.trlc",
        "//score/mw/com/requirements/assumed_system_requirements",
        "//score/mw/com/requirements/feature_requirements:feature_requirements_ipc",
    ],
    spec = [
        "//third_party/traceability/config/trlc/score_model:requirement_metamodel",
    ],
    visibility = ["//visibility:public"],
)

trlc_requirements_test(
    name = "component_requirements_ipc_test",
    reqs = [
        ":component_requirements_ipc",
    ],
    visibility = ["//visibility:public"],
)
