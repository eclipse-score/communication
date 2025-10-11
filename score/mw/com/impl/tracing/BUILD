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
load("@score-baselibs//:bazel/unit_tests.bzl", "cc_gtest_unit_test", "cc_unit_test_suites_for_host_and_qnx")
load("//score/mw:common_features.bzl", "COMPILER_WARNING_FEATURES")

package(
    default_visibility = [
        "//score/mw/com/impl/tracing:__subpackages__",
    ],
)

cc_library(
    name = "type_erased_sample_ptr",
    srcs = ["type_erased_sample_ptr.cpp"],
    hdrs = ["type_erased_sample_ptr.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = ["//score/mw/com/impl:__subpackages__"],
    deps = [
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "i_tracing_runtime_binding",
    srcs = ["i_tracing_runtime_binding.cpp"],
    hdrs = ["i_tracing_runtime_binding.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = ["//score/mw/com/impl:__subpackages__"],
    deps = [
        ":service_element_tracing_data",
        ":type_erased_sample_ptr",
        "@score-baselibs//score/memory/shared",
        "//score/mw/com/impl/tracing/configuration:service_element_instance_identifier_view",
        "@score-baselibs//score/language/futurecpp",
    ] + select({
        "//score/mw/com/flags:tracing_library_real": [
            "@score-baselibs//score/analysis/tracing/generic_trace_library/interface_types:generic_trace_api_header",
        ],

        "//score/mw/com/flags:tracing_library_stub": ["@score-baselibs//score/analysis/tracing/generic_trace_library/stub_implementation:stub_implementation"],
    }),
)

cc_library(
    name = "skeleton_event_tracing_data",
    srcs = ["skeleton_event_tracing_data.cpp"],
    hdrs = ["skeleton_event_tracing_data.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = ["//score/mw/com/impl:__subpackages__"],
    deps = [
        ":service_element_tracing_data",
        "//score/mw/com/impl/tracing/configuration:service_element_instance_identifier_view",
    ],
)

cc_library(
    name = "service_element_tracing_data",
    srcs = ["service_element_tracing_data.cpp"],
    hdrs = ["service_element_tracing_data.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = ["//score/mw/com/impl:__subpackages__"],
    deps = [
        "//score/mw/com/impl/configuration:lola_event_instance_deployment",
    ],
)

cc_library(
    name = "proxy_event_tracing_data",
    srcs = ["proxy_event_tracing_data.cpp"],
    hdrs = ["proxy_event_tracing_data.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = ["//score/mw/com/impl:__subpackages__"],
    deps = [
        "//score/mw/com/impl:event_receive_handler",
        "//score/mw/com/impl:scoped_event_receive_handler",
        "//score/mw/com/impl/tracing/configuration:service_element_instance_identifier_view",
    ],
)

cc_library(
    name = "common_event_tracing",
    srcs = ["common_event_tracing.cpp"],
    hdrs = ["common_event_tracing.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = ["//score/mw/com/impl:__subpackages__"],
    deps = [
        ":tracing_runtime",
        "//score/mw/com/impl:instance_identifier",
        "//score/mw/com/impl:runtime",
        "//score/mw/com/impl:service_element_type",
        "//score/mw/com/impl/tracing/configuration:service_element_instance_identifier_view",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "skeleton_tracing",
    srcs = ["skeleton_tracing.cpp"],
    hdrs = ["skeleton_tracing.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = ["//score/mw/com/impl:__subpackages__"],
    deps = [
        ":tracing_runtime",
        "//score/mw/com/impl:instance_identifier",
        "//score/mw/com/impl:runtime",
        "//score/mw/com/impl:skeleton_binding",
        "//score/mw/com/impl:skeleton_event_base",
        "//score/mw/com/impl:skeleton_field_base",
        "//score/mw/com/impl/tracing/configuration:service_element_instance_identifier_view",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "skeleton_event_tracing",
    srcs = ["skeleton_event_tracing.cpp"],
    hdrs = ["skeleton_event_tracing.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = ["//score/mw/com/impl:__subpackages__"],
    deps = [
        ":common_event_tracing",
        ":skeleton_event_tracing_data",
        ":tracing_runtime",
        "//score/mw/com/impl:binding_type",
        "//score/mw/com/impl:instance_identifier",
        "//score/mw/com/impl:runtime",
        "//score/mw/com/impl:skeleton_event_binding",
        "//score/mw/com/impl/plumbing:sample_allocatee_ptr",
        "//score/mw/com/impl/plumbing:sample_ptr",
        "//score/mw/com/impl/tracing/configuration:service_element_instance_identifier_view",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "proxy_event_tracing",
    srcs = ["proxy_event_tracing.cpp"],
    hdrs = ["proxy_event_tracing.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = ["//score/mw/com/impl:__subpackages__"],
    deps = [
        ":common_event_tracing",
        ":proxy_event_tracing_data",
        ":tracing_runtime",
        "//score/mw/com/impl:generic_proxy_event_binding",
        "//score/mw/com/impl:instance_identifier",
        "//score/mw/com/impl:proxy_event_binding",
        "//score/mw/com/impl:runtime",
        "//score/mw/com/impl/tracing/configuration:service_element_instance_identifier_view",
        "@score-baselibs//score/mw/log",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "trace_error",
    srcs = ["trace_error.cpp"],
    hdrs = ["trace_error.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = ["//score/mw/com/impl:__subpackages__"],
    deps = ["@score-baselibs//score/result"],
)

cc_library(
    name = "i_tracing_runtime",
    srcs = ["i_tracing_runtime.cpp"],
    hdrs = ["i_tracing_runtime.h"],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = [],
    tags = ["FFI"],
    visibility = ["//score/mw/com/impl:__subpackages__"],
    deps = [
        ":i_tracing_runtime_binding",
        ":service_element_tracing_data",
        ":type_erased_sample_ptr",
        "@score-baselibs//score/memory/shared:i_shared_memory_resource",
        "//score/mw/com/impl:binding_type",
        "//score/mw/com/impl/tracing/configuration:proxy_event_trace_point_type",
        "//score/mw/com/impl/tracing/configuration:proxy_field_trace_point_type",
        "//score/mw/com/impl/tracing/configuration:service_element_instance_identifier_view",
        "//score/mw/com/impl/tracing/configuration:skeleton_event_trace_point_type",
        "//score/mw/com/impl/tracing/configuration:skeleton_field_trace_point_type",
        "@score-baselibs//score/language/futurecpp",
    ] + select({
        "//score/mw/com/flags:tracing_library_real": [
        "@score-baselibs//score/analysis/tracing/generic_trace_library/interface_types:trace_library_interface",
        ],

        "//score/mw/com/flags:tracing_library_stub": ["@score-baselibs//score/analysis/tracing/generic_trace_library/stub_implementation:stub_implementation"],
    }),
)

cc_library(
    name = "tracing_runtime_mock",
    testonly = True,
    srcs = ["tracing_runtime_mock.cpp"],
    hdrs = ["tracing_runtime_mock.h"],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = [],
    visibility = ["//score/mw/com/impl:__subpackages__"],
    deps = [
        ":i_tracing_runtime",
        "@googletest//:gtest",
    ],
)

cc_library(
    name = "tracing_runtime",
    srcs = ["tracing_runtime.cpp"],
    hdrs = ["tracing_runtime.h"],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = [
        "@score-baselibs//score/memory/shared:pointer_arithmetic_util",
    ] + select({
        "//score/mw/com/flags:tracing_library_real": [
            "@score-baselibs//score/analysis/tracing/generic_trace_library/interface_types:trace_library_interface",
            "@score-baselibs//score/analysis/tracing/generic_trace_library/stub_implementation:stub_implementation",
        ],

        "//score/mw/com/flags:tracing_library_stub": ["@score-baselibs//score/analysis/tracing/generic_trace_library/stub_implementation:stub_implementation"],
    }),
    tags = ["FFI"],
    visibility = ["//score/mw/com/impl:__subpackages__"],
    deps = [
        ":i_tracing_runtime",
        ":i_tracing_runtime_binding",
        ":trace_error",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "tracing_test_resources",
    testonly = True,
    srcs = ["tracing_test_resources.cpp"],
    hdrs = ["tracing_test_resources.h"],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = [],
    visibility = ["//score/mw/com/impl:__subpackages__"],
    deps = [
        ":i_tracing_runtime_binding",
        ":tracing_runtime",
    ],
)

cc_gtest_unit_test(
    name = "trace_error_test",
    srcs = ["trace_error_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com/impl:__subpackages__",
    ],
    deps = [
        ":trace_error",
    ],
)

cc_gtest_unit_test(
    name = "tracing_runtime_test",
    srcs = [
        "tracing_runtime_test.cpp",
        "tracing_runtime_trace_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com/impl:__subpackages__",
    ],
    deps = [
        ":tracing_runtime",
        ":tracing_test_resources",
        "@score-baselibs//score/analysis/tracing/generic_trace_library/mock:trace_library_mock",
        "//score/mw/com/impl/bindings/mock_binding/tracing:tracing_runtime",
    ],
)

cc_gtest_unit_test(
    name = "type_erased_sample_ptr_test",
    srcs = ["type_erased_sample_ptr_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":type_erased_sample_ptr",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_gtest_unit_test(
    name = "service_element_tracing_data_test",
    srcs = ["service_element_tracing_data_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":service_element_tracing_data",
    ],
)

cc_gtest_unit_test(
    name = "skeleton_tracing_test",
    srcs = ["skeleton_tracing_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":skeleton_tracing",
        ":tracing_runtime_mock",
        "//score/mw/com/impl",
        "//score/mw/com/impl/bindings/mock_binding",
        "//score/mw/com/impl/configuration/test:configuration_store",
        "//score/mw/com/impl/tracing/test:runtime_mock_guard",
    ],
)

cc_gtest_unit_test(
    name = "skeleton_event_tracing_data_test",
    srcs = ["skeleton_event_tracing_data_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":skeleton_event_tracing_data",
    ],
)

cc_gtest_unit_test(
    name = "proxy_event_tracing_data_test",
    srcs = ["proxy_event_tracing_data_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":proxy_event_tracing_data",
    ],
)

cc_gtest_unit_test(
    name = "common_event_tracing_test",
    srcs = ["common_event_tracing_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":common_event_tracing",
        ":tracing_runtime_mock",
        "//score/mw/com/impl/configuration/test:configuration_store",
        "//score/mw/com/impl/tracing/test:runtime_mock_guard",
    ],
)

cc_gtest_unit_test(
    name = "skeleton_event_tracing_test",
    srcs = ["skeleton_event_tracing_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":skeleton_event_tracing",
        ":tracing_runtime_mock",
        "//score/mw/com/impl/bindings/mock_binding",
        "//score/mw/com/impl/configuration/test:configuration_store",
        "//score/mw/com/impl/tracing/configuration:tracing_filter_config_mock",
        "//score/mw/com/impl/tracing/test:runtime_mock_guard",
    ],
)

cc_gtest_unit_test(
    name = "proxy_event_tracing_test",
    srcs = ["proxy_event_tracing_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":proxy_event_tracing",
        ":tracing_runtime_mock",
        "//score/mw/com/impl/bindings/mock_binding",
        "//score/mw/com/impl/tracing/test:runtime_mock_guard",
    ],
)

cc_unit_test_suites_for_host_and_qnx(
    name = "unit_test_suite",
    cc_unit_tests = [
        ":common_event_tracing_test",
        ":proxy_event_tracing_data_test",
        ":proxy_event_tracing_test",
        ":service_element_tracing_data_test",
        ":skeleton_event_tracing_data_test",
        ":skeleton_event_tracing_test",
        ":skeleton_tracing_test",
        ":trace_error_test",
        ":tracing_runtime_test",
        ":type_erased_sample_ptr_test",
    ],
    test_suites_from_sub_packages = [
        "//score/mw/com/impl/tracing/configuration:unit_test_suite",
        "//score/mw/com/impl/tracing/test:unit_test_suite",
    ],
    visibility = ["//score/mw/com/impl:__pkg__"],
)
