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

load("@trlc//:trlc.bzl", "trlc_requirements", "trlc_requirements_test", "trlc_specification")

trlc_specification(
    name = "requirements_specification",
    srcs = ["requirements_model.rsl"],
    visibility = ["//visibility:public"],
)

trlc_requirements(
    name = "comp_req_lola",
    srcs = [
        "comp_req_lola.trlc",
        "@score_platform//docs/features/communication/docs/requirements:feat_req_communication",
    ],
    spec = [
        "@score_platform//docs:requirements_metamodel",
        "@score_platform//docs:architecture_metamodel",
        "@score_platform//docs:score_types",
    ],
    visibility = ["//visibility:public"],
)

trlc_requirements(
    name = "comp_arch_lola",
    srcs = [
        "comp_arch_lola.trlc",
        "@score_platform//docs/features/communication/docs/requirements:feat_req_communication",
    ],
    spec = [
        "@score_platform//docs:requirements_metamodel",
        "@score_platform//docs:architecture_metamodel",
        "@score_platform//docs:score_types",
    ],
    visibility = ["//visibility:public"],
)

trlc_requirements_test(
    name = "comp_req_lola_test",
    reqs = [":comp_req_lola"]
)

trlc_requirements_test(
    name = "comp_arch_lola_test",
    reqs = [":comp_arch_lola"]
)
