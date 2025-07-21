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


trlc_requirements(
    name = "requirements",
    srcs = [
        "assumed_functional_requirements.trlc",
        "assumed_technical_safety_requirements.trlc"
    ],
    spec = [
        "//score/mw/com/doc:requirements_specification",
    ],
    visibility = ["//visibility:public"],
)


trlc_requirements_test(
    name = "requirements_tests",
    reqs = [":requirements"]
)
