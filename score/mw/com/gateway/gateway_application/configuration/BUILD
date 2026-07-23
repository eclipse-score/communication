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
load("//:score/common_features.bzl", "COMPILER_WARNING_FEATURES")
load("//bazel/tools:json_schema_validator.bzl", "validate_json_schema_test")
load("//quality/unit_testing:unit_testing.bzl", "cc_unit_test")

package(
    default_visibility = ["//score/mw/com/gateway:__subpackages__"],
)

validate_json_schema_test(
    name = "validate_gateway_config_schema",
    json = "example/mw_com_gateway_config.json",
    schema = ":mw_com_gateway_config_schema",
    tags = ["lint"],
)

filegroup(
    name = "mw_com_gateway_config_schema",
    srcs = ["mw_com_gateway_config_schema.json"],
)

cc_library(
    name = "gateway_configuration",
    srcs = ["gateway_configuration.cpp"],
    hdrs = ["gateway_configuration.h"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        "@score_baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "gateway_config_parser",
    srcs = ["gateway_config_parser.cpp"],
    hdrs = ["gateway_config_parser.h"],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = [
        "@score_baselibs//score/mw/log",
    ],
    deps = [
        ":gateway_configuration",
        "@score_baselibs//score/json",
    ],
)

cc_unit_test(
    name = "gateway_config_parser_test",
    srcs = ["gateway_config_parser_test.cpp"],
    data = ["example/mw_com_gateway_config.json"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":gateway_config_parser",
        "@score_baselibs//score/json",
    ],
)
