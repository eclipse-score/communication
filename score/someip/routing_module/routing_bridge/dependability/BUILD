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
load("@score_tooling//bazel/rules/rules_score:rules_score.bzl", "unit_design")

unit_design(
    name = "routing_bridge_unit_design",
    dynamic = [
        "routing_bridge_sequence.puml",
    ],
    static = [
        "routing_bridge_class.puml",
        "routing_bridge_class.rst",
    ],
    visibility = ["//score/someip/routing_module/routing_bridge:__pkg__"],
)
