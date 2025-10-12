load("@score-baselibs//:bazel/unit_tests.bzl", "cc_gtest_unit_test", "cc_unit_test_suites_for_host_and_qnx")

# *******************************************************************************
# Copyright (c) 2025 Contributors to the Eclipse Foundation
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
load("//bazel/tools:json_schema_validator.bzl", "validate_json_schema_test")
load("//score/mw:common_features.bzl", "COMPILER_WARNING_FEATURES")

validate_json_schema_test(
    name = "validate_comtrace_filter_config_schema",
    json = "example/comtrace_filter_config_small.json",
    schema = ":comtrace_filter_config_schema",
    tags = ["lint"],
)

# schema will be moved later to @score-baselibs//score/analysis/common
filegroup(
    name = "comtrace_filter_config_schema",
    srcs = ["comtrace_config_schema.json"],
    visibility = [
        "//score/mw/com:__pkg__",
        "//score/mw/com/impl/tracing/configuration:__pkg__",
    ],
)

cc_library(
    name = "hash_helper_utility",
    srcs = ["hash_helper_utility.cpp"],
    hdrs = ["hash_helper_utility.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = ["//score/mw/com/impl/tracing/configuration:__subpackages__"],
)

cc_library(
    name = "i_tracing_filter_config",
    srcs = ["i_tracing_filter_config.cpp"],
    hdrs = ["i_tracing_filter_config.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = ["//score/mw/com/impl:__subpackages__"],
    deps = [
        "proxy_event_trace_point_type",
        "proxy_field_trace_point_type",
        "skeleton_event_trace_point_type",
        "skeleton_field_trace_point_type",
        "//score/mw/com/impl/configuration",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "tracing_filter_config",
    srcs = ["tracing_filter_config.cpp"],
    hdrs = ["tracing_filter_config.h"],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = ["@score-baselibs//score/mw/log"],
    tags = ["FFI"],
    visibility = ["//score/mw/com/impl:__subpackages__"],
    deps = [
        ":hash_helper_utility",
        ":i_tracing_filter_config",
        ":service_element_identifier_view",
        ":trace_point_key",
        "//score/mw/com/impl:service_element_type",
        "//score/mw/com/impl/configuration",
    ],
)

cc_library(
    name = "tracing_filter_config_mock",
    testonly = True,
    srcs = ["tracing_filter_config_mock.cpp"],
    hdrs = ["tracing_filter_config_mock.h"],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = ["@score-baselibs//score/mw/log"],
    visibility = ["//score/mw/com/impl:__subpackages__"],
    deps = [
        ":i_tracing_filter_config",
        "@googletest//:gtest",
    ],
)

cc_library(
    name = "tracing_config",
    srcs = ["tracing_config.cpp"],
    hdrs = ["tracing_config.h"],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = ["@score-baselibs//score/mw/log"],
    tags = ["FFI"],
    visibility = [
        "//score/mw/com/impl/configuration:__pkg__",
    ],
)

cc_library(
    name = "hash_helper_for_service_element_and_se_view",
    srcs = ["hash_helper_for_service_element_and_se_view.cpp"],
    hdrs = ["hash_helper_for_service_element_and_se_view.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = ["//score/mw/com/impl:__subpackages__"],
    deps = [
        ":hash_helper_utility",
        "@score-baselibs//score/language/futurecpp",
        "@score-baselibs//score/mw/log",
    ],
)

cc_library(
    name = "service_element_identifier",
    srcs = ["service_element_identifier.cpp"],
    hdrs = ["service_element_identifier.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = ["//score/mw/com/impl:__subpackages__"],
    deps = [
        ":hash_helper_for_service_element_and_se_view",
        "//score/mw/com/impl:service_element_type",
        "@score-baselibs//score/language/futurecpp",
        "@score-baselibs//score/mw/log",
    ],
)

cc_library(
    name = "service_element_identifier_view",
    srcs = ["service_element_identifier_view.cpp"],
    hdrs = ["service_element_identifier_view.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = ["//score/mw/com/impl:__subpackages__"],
    deps = [
        ":hash_helper_for_service_element_and_se_view",
        "//score/mw/com/impl:service_element_type",
        "@score-baselibs//score/language/futurecpp",
        "@score-baselibs//score/mw/log",
    ],
)

cc_library(
    name = "service_element_instance_identifier_view",
    srcs = ["service_element_instance_identifier_view.cpp"],
    hdrs = ["service_element_instance_identifier_view.h"],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = [
        "@score-baselibs//score/mw/log",
    ],
    tags = ["FFI"],
    visibility = ["//score/mw/com/impl:__subpackages__"],
    deps = [
        ":hash_helper_utility",
        ":service_element_identifier_view",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "trace_point_key",
    srcs = ["trace_point_key.cpp"],
    hdrs = ["trace_point_key.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com/impl/tracing:__subpackages__",
    ],
    deps = [
        ":hash_helper_utility",
        ":service_element_identifier_view",
        "@score-baselibs//score/mw/log",
    ],
)

cc_library(
    name = "skeleton_event_trace_point_type",
    srcs = ["skeleton_event_trace_point_type.cpp"],
    hdrs = ["skeleton_event_trace_point_type.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = ["//score/mw/com/impl:__subpackages__"],
)

cc_library(
    name = "skeleton_field_trace_point_type",
    srcs = ["skeleton_field_trace_point_type.cpp"],
    hdrs = ["skeleton_field_trace_point_type.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = ["//score/mw/com/impl:__subpackages__"],
)

cc_library(
    name = "proxy_event_trace_point_type",
    srcs = ["proxy_event_trace_point_type.cpp"],
    hdrs = ["proxy_event_trace_point_type.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = ["//score/mw/com/impl:__subpackages__"],
)

cc_library(
    name = "proxy_field_trace_point_type",
    srcs = ["proxy_field_trace_point_type.cpp"],
    hdrs = ["proxy_field_trace_point_type.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = ["//score/mw/com/impl:__subpackages__"],
)

cc_library(
    name = "tracing_filter_config_parser",
    srcs = ["tracing_filter_config_parser.cpp"],
    hdrs = ["tracing_filter_config_parser.h"],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = [
        "@score-baselibs//score/mw/log",
    ],
    tags = ["FFI"],
    visibility = ["//score/mw/com/impl:__subpackages__"],
    deps = [
        ":tracing_filter_config",
        "//score/mw/com/impl:service_element_type",
        "//score/mw/com/impl/configuration",
        "//score/mw/com/impl/tracing:trace_error",
        "@score-baselibs//score/json",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_gtest_unit_test(
    name = "service_element_identifier_test",
    srcs = ["service_element_identifier_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com/impl/tracing:__subpackages__",
    ],
    deps = [
        ":service_element_identifier",
        "//score/mw/com/impl:service_element_type",
    ],
)

cc_gtest_unit_test(
    name = "service_element_identifier_view_test",
    srcs = ["service_element_identifier_view_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com/impl/tracing:__subpackages__",
    ],
    deps = [
        ":service_element_identifier_view",
        "//score/mw/com/impl:service_element_type",
    ],
)

cc_gtest_unit_test(
    name = "service_element_instance_identifier_view_test",
    srcs = ["service_element_instance_identifier_view_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com/impl/tracing:__subpackages__",
    ],
    deps = [
        ":service_element_instance_identifier_view",
    ],
)

cc_gtest_unit_test(
    name = "trace_point_key_test",
    srcs = ["trace_point_key_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com/impl/tracing:__subpackages__",
    ],
    deps = [
        ":service_element_identifier",
        ":trace_point_key",
        "//score/mw/com/impl:service_element_type",
    ],
)

cc_gtest_unit_test(
    name = "tracing_filter_config_test",
    srcs = ["tracing_filter_config_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com/impl/tracing:__subpackages__",
    ],
    deps = [
        ":tracing_filter_config",
        "//score/mw/com/impl/configuration",
    ],
)

cc_gtest_unit_test(
    name = "skeleton_event_trace_point_type_test",
    srcs = ["skeleton_event_trace_point_type_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com/impl/tracing:__subpackages__",
    ],
    deps = [
        ":skeleton_event_trace_point_type",
    ],
)

cc_gtest_unit_test(
    name = "skeleton_field_trace_point_type_test",
    srcs = ["skeleton_field_trace_point_type_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com/impl/tracing:__subpackages__",
    ],
    deps = [
        ":skeleton_field_trace_point_type",
    ],
)

cc_gtest_unit_test(
    name = "proxy_event_trace_point_type_test",
    srcs = ["proxy_event_trace_point_type_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com/impl/tracing:__subpackages__",
    ],
    deps = [
        ":proxy_event_trace_point_type",
    ],
)

cc_gtest_unit_test(
    name = "proxy_field_trace_point_type_test",
    srcs = ["proxy_field_trace_point_type_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com/impl/tracing:__subpackages__",
    ],
    deps = [
        ":proxy_field_trace_point_type",
    ],
)

cc_gtest_unit_test(
    name = "tracing_filter_config_parser_test",
    srcs = ["tracing_filter_config_parser_test.cpp"],
    data = ["example/comtrace_filter_config_small.json"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com/impl/tracing:__subpackages__",
    ],
    deps = [
        ":tracing_filter_config_mock",
        ":tracing_filter_config_parser",
        "//score/mw/com/impl/tracing/test:runtime_mock_guard",
    ],
)

cc_gtest_unit_test(
    name = "hash_helper_utility_test",
    srcs = ["hash_helper_utility_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com/impl/tracing:__subpackages__",
    ],
    deps = [
        ":hash_helper_utility",
    ],
)

cc_unit_test_suites_for_host_and_qnx(
    name = "unit_test_suite",
    cc_unit_tests = [
        ":hash_helper_utility_test",
        ":proxy_event_trace_point_type_test",
        ":proxy_field_trace_point_type_test",
        ":service_element_identifier_test",
        ":service_element_identifier_view_test",
        ":service_element_instance_identifier_view_test",
        ":skeleton_event_trace_point_type_test",
        ":skeleton_field_trace_point_type_test",
        ":trace_point_key_test",
        ":tracing_filter_config_parser_test",
        ":tracing_filter_config_test",
    ],
    visibility = ["//score/mw/com/impl/tracing:__pkg__"],
)

filegroup(
    name = "comtrace_filter_config_broken.json",
    srcs = ["example/comtrace_filter_config_broken.json"],
    visibility = ["//visibility:public"],
)

filegroup(
    name = "comtrace_filter_config_small.json",
    srcs = ["example/comtrace_filter_config_small.json"],
    visibility = ["//visibility:public"],
)
