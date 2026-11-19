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

load("@rules_cc//cc:defs.bzl", "cc_library")
load("@score_tooling//bazel/rules/rules_score:rules_score.bzl", "component", "unit")
load("//quality/unit_testing:unit_testing.bzl", "cc_unit_test")

cc_library(
    name = "wire_format",
    hdrs = ["wire_format.h"],
    visibility = ["//visibility:private"],
)

cc_library(
    name = "router_control_protocol",
    srcs = ["router_control_protocol.cpp"],
    hdrs = ["router_control_protocol.h"],
    visibility = ["//score/someip:__subpackages__"],
    deps = [
        ":wire_format",
        "@score_baselibs//score/language/futurecpp",
    ],
)

cc_unit_test(
    name = "router_control_protocol_test",
    srcs = ["router_control_protocol_test.cpp"],
    visibility = ["//score/someip:__subpackages__"],
    deps = [
        ":router_control_protocol",
        ":wire_format",
    ],
)

unit(
    name = "router_control_codec",
    design_name = "RouterControlCodec",
    maturity = "development",
    scope = ["//score/someip/router_control_protocol:__subpackages__"],
    target_compatible_with = ["@platforms//os:linux"],
    test_specs = [],
    tests = [":router_control_protocol_test_linux"],
    unit_design = ["//score/someip/router_control_protocol/dependability:router_control_codec_unit_design"],
    visibility = ["//score/someip/dependability:__subpackages__"],
    implementation = [":router_control_protocol"],
)

component(
    name = "router_control",
    components = [":router_control_codec"],
    design_name = "RouterControlProtocol",
    requirements = [
        "//score/someip/router_control_protocol/requirements/component_requirements:component_requirements",
        "//score/someip/dependability/requirements/feature_requirements:feature_requirements_routing_module",
    ],
    tags = ["manual"],
    target_compatible_with = ["@platforms//os:linux"],
    test_case_coverage_lock = "test_case_coverage.lock.yaml",
    tests = [],
    visibility = ["//score/someip/dependability:__pkg__"],
)
