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

load("@trlc//:trlc.bzl", "trlc_requirements_test", "trlc_specification")

trlc_specification(
    name = "requirement_metamodel",
    srcs = [
        "score_requirements_model.rsl",
    ],
    visibility = ["//visibility:public"],
)

trlc_requirements_test(
    name = "requirement_metamodel_test",
    reqs = [
        ":requirement_metamodel",
    ],
)
