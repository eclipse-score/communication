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

load(
    "@score_tooling//bazel/rules/rules_score:rules_score.bzl",
    "component",
    "dependable_element",
    "unit",
)

def score_baselibs_stub(
        name,
        scope_path,
        integrity_level = "B",
        maturity = "development",
        visibility = ["//visibility:public"]):
    """Creates certified-scope stubs (unit + component + dependable_element) for a score_baselibs package.

    Args:
        name:            Base name; produces <name>_unit, <name>_component, and <name> targets.
        scope_path:      The Bazel label pattern passed to unit.scope, e.g.
                         "@@score_baselibs+//score/containers:__subpackages__".
        integrity_level: Forwarded to dependable_element (default "B").
        maturity:        Forwarded to dependable_element (default "development").
        visibility:      Forwarded to dependable_element (default public).
    """
    unit(
        name = name + "_unit",
        scope = [scope_path],
        tests = [],
        unit_design = [],
        implementation = [],
    )

    component(
        name = name + "_component",
        components = [":" + name + "_unit"],
    )

    dependable_element(
        name = name,
        architectural_design = [],
        assumptions_of_use = [],
        components = [":" + name + "_component"],
        dependability_analysis = [],
        integrity_level = integrity_level,
        maturity = maturity,
        requirements = [],
        tests = [],
        visibility = visibility,
    )
