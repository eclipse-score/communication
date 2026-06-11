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
load("//bazel/tools:json_schema_validator.bzl", "validate_json_schema_test")
load("//quality/unit_testing:unit_testing.bzl", "cc_unit_test")
load("//score/mw:common_features.bzl", "COMPILER_WARNING_FEATURES")

package(
    default_visibility = ["//score/mw/com/gateway:__subpackages__"],
)

validate_json_schema_test(
    name = "validate_sample_transport_config_schema",
    json = "example/mw_com_gateway_sample_transport_config.json",
    schema = ":mw_com_gateway_sample_transport_config_schema",
    tags = ["lint"],
)

filegroup(
    name = "mw_com_gateway_sample_transport_config_schema",
    srcs = ["mw_com_gateway_sample_transport_config_schema.json"],
)

cc_library(
    name = "hypervisor_socket_configuration",
    hdrs = ["hypervisor_socket_configuration.h"],
    features = COMPILER_WARNING_FEATURES,
    deps = ["@score_baselibs//score/network:ipv4_address"],
)

cc_library(
    name = "sample_transport_config_parser",
    srcs = ["sample_transport_config_parser.cpp"],
    hdrs = ["sample_transport_config_parser.h"],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = [
        "@score_baselibs//score/mw/log",
    ],
    deps = [
        ":hypervisor_socket_configuration",
        "@score_baselibs//score/json",
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/network:ipv4_address",
    ],
)

cc_unit_test(
    name = "sample_transport_config_parser_test",
    srcs = ["sample_transport_config_parser_test.cpp"],
    data = ["example/mw_com_gateway_sample_transport_config.json"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":sample_transport_config_parser",
        "@score_baselibs//score/json",
    ],
)
