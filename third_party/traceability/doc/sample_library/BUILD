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

load("//third_party/traceability/bazel:traceability.bzl", "safety_software_seooc", "safety_software_unit")

cc_library(
    name = "sample_library",
    visibility = ["@//visibility:public"],
    deps = [
        "@//third_party/traceability/doc/sample_library/unit_1",
        "@//third_party/traceability/doc/sample_library/unit_2",
    ],
)

safety_software_unit(
    name = "safety_software_unit_example",
    design = ["@//third_party/traceability/doc/sample_library/design:unit_design"],
    impl = ["sample_library"],
    reqs = [
        "@//third_party/traceability/doc/sample_library/requirements:component_requirements",
    ],
    tests = [
        "@//third_party/traceability/doc/sample_library/unit_1:unit_1_test",
        "@//third_party/traceability/doc/sample_library/unit_2:unit_2_test",
    ],
)

safety_software_seooc(
    name = "safety_software_seooc_example",
    analysis = None,
    design = "",
    reqs = "",
    tests = "",
)
