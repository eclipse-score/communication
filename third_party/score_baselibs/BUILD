load(
    "@score_tooling//bazel/rules/rules_score:rules_score.bzl",
    "component",
    "dependable_element",
    "unit",
)

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
exports_files(["restore_qnx8_poll_workaround.patch"])

# Certified-scope stubs for score_baselibs dependencies used by mw_com.
# Each stub declares the scope of one package group so the dependable_element
# validation does not emit "Not in certified scope" warnings.

unit(
    name = "containers_unit",
    scope = ["@@score_baselibs+//score/containers:__subpackages__"],
    tests = [],
    unit_design = [],
    implementation = [],
)

component(
    name = "containers_component",
    components = [":containers_unit"],
)

dependable_element(
    name = "containers",
    architectural_design = [],
    assumptions_of_use = [],
    components = [":containers_component"],
    dependability_analysis = [],
    integrity_level = "B",
    maturity = "development",
    requirements = [],
    tests = [],
    visibility = ["//visibility:public"],
)

unit(
    name = "futurecpp_unit",
    scope = ["@@score_baselibs+//score/language/futurecpp:__subpackages__"],
    tests = [],
    unit_design = [],
    implementation = [],
)

component(
    name = "futurecpp_component",
    components = [":futurecpp_unit"],
)

dependable_element(
    name = "futurecpp",
    architectural_design = [],
    assumptions_of_use = [],
    components = [":futurecpp_component"],
    dependability_analysis = [],
    integrity_level = "B",
    maturity = "development",
    requirements = [],
    tests = [],
    visibility = ["//visibility:public"],
)

# scope covers score/os AND score/os/utils via __subpackages__
unit(
    name = "os_unit",
    scope = ["@@score_baselibs+//score/os:__subpackages__"],
    tests = [],
    unit_design = [],
    implementation = [],
)

component(
    name = "os_component",
    components = [":os_unit"],
)

dependable_element(
    name = "os",
    architectural_design = [],
    assumptions_of_use = [],
    components = [":os_component"],
    dependability_analysis = [],
    integrity_level = "B",
    maturity = "development",
    requirements = [],
    tests = [],
    visibility = ["//visibility:public"],
)

unit(
    name = "bitmanipulation_unit",
    scope = ["@@score_baselibs+//score/bitmanipulation:__subpackages__"],
    tests = [],
    unit_design = [],
    implementation = [],
)

component(
    name = "bitmanipulation_component",
    components = [":bitmanipulation_unit"],
)

dependable_element(
    name = "bitmanipulation",
    architectural_design = [],
    assumptions_of_use = [],
    components = [":bitmanipulation_component"],
    dependability_analysis = [],
    integrity_level = "B",
    maturity = "development",
    requirements = [],
    tests = [],
    visibility = ["//visibility:public"],
)
