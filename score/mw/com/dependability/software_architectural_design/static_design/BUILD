# *******************************************************************************
# Copyright (c) 2026 Contributors to the Eclipse Foundation
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
load("@score_tooling//bazel/rules/rules_score:rules_score.bzl", "architectural_design")

architectural_design(
    name = "software_architectural_design",
    internal_api = [
    ],
    static = [
        "overall.puml",
        "binding_independent.puml",
        "lola_binding.puml",
        "someip_binding.puml",
        "mock_binding.puml"    ],
    visibility = ["//score/mw/com/dependability:__subpackages__"],
)

architectural_design(
    name = "interface_design",
    internal_api = [
        "//score/mw/com/dependability/software_architectural_design/static_design/interfaces/internal_interfaces:internal_interfaces",
    ],
    static = [
        "//score/mw/com/dependability/software_architectural_design/static_design/interfaces/component_interactions:component_interactions",
    ],
    visibility = ["//score/mw/com/dependability:__subpackages__"],
)
