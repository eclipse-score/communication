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
load("@score_tooling//bazel/rules/rules_score:rules_score.bzl", "unit")
load("//quality/unit_testing:unit_testing.bzl", "cc_unit_test")
load("//score/mw:common_features.bzl", "COMPILER_WARNING_FEATURES")

package(default_visibility = ["//score/mw/com:__subpackages__"])

cc_library(
    name = "service_discovery_protocol",
    srcs = ["service_discovery_protocol.cpp"],
    hdrs = [
        "service_discovery_protocol.h",
        "service_discovery_types.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    deps = ["@score_baselibs//score/language/futurecpp"],
)

cc_library(
    name = "service_discovery_registry",
    srcs = ["service_discovery_registry.cpp"],
    hdrs = [
        "i_service_discovery_registry.h",
        "service_discovery_registry.h",
        "service_discovery_types.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
)

cc_library(
    name = "service_discovery_daemon",
    srcs = ["service_discovery_daemon.cpp"],
    hdrs = ["service_discovery_daemon.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    deps = [
        ":service_discovery_protocol",
        ":service_discovery_registry",
    ],
)

cc_library(
    name = "service_discovery_message_passing_server",
    srcs = ["service_discovery_message_passing_server.cpp"],
    hdrs = ["service_discovery_message_passing_server.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    deps = [
        ":service_discovery_daemon",
        "//score/message_passing",
    ],
)

cc_library(
    name = "service_discovery_message_passing_client",
    srcs = ["service_discovery_message_passing_client.cpp"],
    hdrs = ["service_discovery_message_passing_client.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    deps = [
        ":service_discovery_protocol",
        "//score/message_passing",
    ],
)

cc_library(
    name = "service_discovery_compat",
    srcs = ["service_discovery_compat.cpp"],
    hdrs = ["service_discovery_compat.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = ["//score/mw/com/impl:__subpackages__"],
    deps = [
        ":service_discovery_message_passing_client",
        "//score/mw/com/impl:enriched_instance_identifier",
        "//score/mw/com/impl:error",
        "//score/mw/com/impl:find_service_handle",
        "//score/mw/com/impl:find_service_handler",
        "//score/mw/com/impl:handle_type",
        "//score/mw/com/impl:i_service_discovery",
        "//score/mw/com/impl:instance_identifier",
        "//score/mw/com/impl:instance_specifier",
        "//score/mw/com/impl:runtime_interfaces",
        "//score/mw/com/impl/configuration",
        "//score/mw/com/impl/configuration:lola_service_instance_id",
        "//score/mw/com/impl/configuration:lola_service_type_deployment",
        "//score/mw/com/impl/configuration:quality_type",
        "//score/mw/com/impl/configuration:service_instance_id",
    ],
)

cc_unit_test(
    name = "service_discovery_protocol_test",
    srcs = ["service_discovery_protocol_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    deps = [":service_discovery_protocol"],
)

cc_unit_test(
    name = "service_discovery_registry_test",
    srcs = ["service_discovery_registry_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    deps = [":service_discovery_registry"],
)

cc_unit_test(
    name = "service_discovery_daemon_test",
    srcs = ["service_discovery_daemon_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":service_discovery_daemon",
        ":service_discovery_protocol",
        ":service_discovery_registry",
    ],
)

cc_unit_test(
    name = "service_discovery_message_passing_integration_test",
    srcs = ["service_discovery_message_passing_integration_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":service_discovery_daemon",
        ":service_discovery_message_passing_client",
        ":service_discovery_message_passing_server",
        ":service_discovery_registry",
    ],
)

cc_unit_test(
    name = "service_discovery_compat_test",
    srcs = ["service_discovery_compat_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":service_discovery_compat",
        ":service_discovery_daemon",
        ":service_discovery_message_passing_server",
        ":service_discovery_registry",
        "//score/mw/com/impl:runtime_mock",
    ],
)

unit(
    name = "service_discovery_protocol_unit",
    scope = [
        "@@//score/mw/com/service_discovery:__subpackages__",
    ],
    tests = [
        ":service_discovery_protocol_test_linux",
    ],
    unit_design = ["//score/mw/com/service_discovery/dependability/software_unit_design:service_discovery_protocol_unit_design"],
    visibility = ["//score/mw/com/service_discovery/dependability:__pkg__"],
    implementation = [":service_discovery_protocol"],
)

unit(
    name = "service_discovery_registry_unit",
    scope = [
        "@@//score/mw/com/service_discovery:__subpackages__",
    ],
    tests = [
        ":service_discovery_registry_test_linux",
    ],
    unit_design = ["//score/mw/com/service_discovery/dependability/software_unit_design:service_discovery_registry_unit_design"],
    visibility = ["//score/mw/com/service_discovery/dependability:__pkg__"],
    implementation = [":service_discovery_registry"],
)

unit(
    name = "service_discovery_daemon_unit",
    scope = [
        "@@//score/mw/com/service_discovery:__subpackages__",
    ],
    tests = [
        ":service_discovery_daemon_test_linux",
    ],
    unit_design = ["//score/mw/com/service_discovery/dependability/software_unit_design:service_discovery_daemon_unit_design"],
    visibility = ["//score/mw/com/service_discovery/dependability:__pkg__"],
    implementation = [":service_discovery_daemon"],
)

unit(
    name = "service_discovery_message_passing_client_unit",
    scope = [
        "@@//score/mw/com/service_discovery:__subpackages__",
    ],
    tests = [],
    unit_design = ["//score/mw/com/service_discovery/dependability/software_unit_design:service_discovery_message_passing_client_unit_design"],
    visibility = ["//score/mw/com/service_discovery/dependability:__pkg__"],
    implementation = [":service_discovery_message_passing_client"],
)

unit(
    name = "service_discovery_message_passing_server_unit",
    scope = [
        "@@//score/mw/com/service_discovery:__subpackages__",
    ],
    tests = [],
    unit_design = ["//score/mw/com/service_discovery/dependability/software_unit_design:service_discovery_message_passing_server_unit_design"],
    visibility = ["//score/mw/com/service_discovery/dependability:__pkg__"],
    implementation = [":service_discovery_message_passing_server"],
)
