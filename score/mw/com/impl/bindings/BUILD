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
load("@score_tooling//bazel/rules/rules_score:rules_score.bzl", "component")

component(
    name = "bindings",
    components = [
        "//score/mw/com/impl/bindings/lola:lola_component",
        "//score/mw/com/impl/bindings/mock_binding:mock_component",
    ],
    requirements = [],
    tests = [],
    visibility = ["//score/mw/com/dependability:__pkg__"],
)
