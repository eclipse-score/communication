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
load(
    "@score_tooling//bazel/rules/rules_score:rules_score.bzl",
    "component",
)

component(
    name = "binding_independent",
    components = [
        "//score/mw/com/impl_new/binding_independent/skeleton:skeleton_component",
    ],
    requirements = [
        # "//score/mw/com/impl_new/binding_independent/skeleton/skeleton/docs/requirements:component_requirements",
    ],
    # tags = ["manual"],
    # test_case_coverage_lock = "test_case_coverage.lock.yaml",
    tests = [],
    visibility = ["//score/mw/com/dependability:__subpackages__"],
)
