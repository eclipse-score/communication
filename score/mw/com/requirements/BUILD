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

load("//third_party/traceability/bazel:traceability.bzl", "safety_software_unit")

safety_software_unit(
    name = "safety_software_unit_com",
    # design = ["*.puml"], TODO: Add also design parts, work based on Zhaoxiong
    impl = ["//score/mw/com"],
    reqs = [
        "//score/mw/com/requirements/component_requirements:component_requirements_ipc",
    ],
    tests = [
        "//score/mw/com:unit_test",
    ],
)
