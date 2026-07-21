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
    name = "mw_com_software_architectural_design",
    dynamic = [],
    # internal_api = ["internal_api/api_definition/lola_proxy_internal_api.puml"],
    public_api = [],
    static = ["static_view.puml"],
    visibility = ["//score/mw/com/dependability:__subpackages__"],
)

# component(
#     name         = "binding_independent",
#     #requirements = [":component_requirements"],
#     #components   = [":unit_1", ":unit_2"],
#     tests        = [],
# )
