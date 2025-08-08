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
load("@rules_rust//rust:defs.bzl", "rust_library")
load("@score-baselibs//bazel:unit_tests.bzl", "cc_gtest_unit_test", "cc_unit_test_suites_for_host_and_qnx")
load("//score/mw:common_features.bzl", "COMPILER_WARNING_FEATURES")

cc_library(
    name = "plumbing",
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com/impl:__subpackages__",
    ],
    deps = [
        ":proxy_binding_factory",
        ":runtime_binding_factory",
        ":sample_allocatee_ptr",
        ":sample_ptr",
        ":skeleton_binding_factory",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "runtime_binding_factory",
    srcs = ["runtime_binding_factory.cpp"],
    hdrs = ["runtime_binding_factory.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com/impl/plumbing:__subpackages__",
    ],
    deps = [
        "@score-baselibs//score/concurrency:executor",
        "//score/mw/com/impl:instance_identifier",
        "//score/mw/com/impl/bindings/lola",
        "//score/mw/com/impl/configuration",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "i_proxy_binding_factory",
    srcs = ["i_proxy_binding_factory.cpp"],
    hdrs = ["i_proxy_binding_factory.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com/impl/plumbing:__subpackages__",
    ],
    deps = [
        "//score/mw/com/impl:handle_type",
        "//score/mw/com/impl:instance_identifier",
        "//score/mw/com/impl:proxy_binding",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "i_proxy_event_binding_factory",
    srcs = ["i_proxy_event_binding_factory.cpp"],
    hdrs = ["i_proxy_event_binding_factory.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com/impl/plumbing:__subpackages__",
    ],
    deps = [
        "//score/mw/com/impl:instance_identifier",
        "//score/mw/com/impl:proxy_base",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "i_proxy_field_binding_factory",
    srcs = ["i_proxy_field_binding_factory.cpp"],
    hdrs = ["i_proxy_field_binding_factory.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com/impl/plumbing:__subpackages__",
    ],
    deps = [
        "//score/mw/com/impl:instance_identifier",
        "//score/mw/com/impl:proxy_base",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "i_skeleton_binding_factory",
    srcs = ["i_skeleton_binding_factory.cpp"],
    hdrs = ["i_skeleton_binding_factory.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com/impl/plumbing:__subpackages__",
    ],
    deps = [
        "//score/mw/com/impl:instance_identifier",
        "//score/mw/com/impl:skeleton_binding",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "i_skeleton_event_binding_factory",
    srcs = ["i_skeleton_event_binding_factory.cpp"],
    hdrs = ["i_skeleton_event_binding_factory.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com/impl/plumbing:__subpackages__",
    ],
    deps = [
        "//score/mw/com/impl:instance_identifier",
        "//score/mw/com/impl:skeleton_base",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "i_skeleton_field_binding_factory",
    srcs = ["i_skeleton_field_binding_factory.cpp"],
    hdrs = ["i_skeleton_field_binding_factory.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com/impl/plumbing:__subpackages__",
    ],
    deps = [
        "//score/mw/com/impl:instance_identifier",
        "//score/mw/com/impl:skeleton_base",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "proxy_binding_factory_impl",
    srcs = ["proxy_binding_factory_impl.cpp"],
    hdrs = ["proxy_binding_factory_impl.h"],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = ["//score/mw/com/impl/bindings/lola"],
    tags = ["FFI"],
    visibility = [
        "//score/mw/com/impl/plumbing:__subpackages__",
    ],
    deps = [
        ":i_proxy_binding_factory",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "proxy_service_element_binding_factory_impl",
    srcs = ["proxy_service_element_binding_factory_impl.cpp"],
    hdrs = ["proxy_service_element_binding_factory_impl.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com/impl/plumbing:__subpackages__",
    ],
    deps = [
        "//score/mw/com/impl:proxy_base",
        "//score/mw/com/impl/bindings/lola",
        "//score/mw/com/impl/configuration",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "proxy_event_binding_factory_impl",
    srcs = ["proxy_event_binding_factory_impl.cpp"],
    hdrs = ["proxy_event_binding_factory_impl.h"],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = ["//score/mw/com/impl/bindings/lola"],
    tags = ["FFI"],
    visibility = [
        "//score/mw/com/impl/plumbing:__subpackages__",
    ],
    deps = [
        ":i_proxy_event_binding_factory",
        ":proxy_service_element_binding_factory_impl",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "proxy_field_binding_factory_impl",
    srcs = ["proxy_field_binding_factory_impl.cpp"],
    hdrs = ["proxy_field_binding_factory_impl.h"],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = ["//score/mw/com/impl/bindings/lola"],
    tags = ["FFI"],
    deps = [
        ":i_proxy_field_binding_factory",
        ":proxy_service_element_binding_factory_impl",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "skeleton_binding_factory_impl",
    srcs = ["skeleton_binding_factory_impl.cpp"],
    hdrs = ["skeleton_binding_factory_impl.h"],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = [
        "//score/mw/com/impl/bindings/lola",
        "//score/mw/com/impl/bindings/lola:partial_restart_path_builder",
        "//score/mw/com/impl/bindings/lola:shm_path_builder",
        "@score-baselibs//score/mw/log",
    ],
    tags = ["FFI"],
    deps = [
        ":i_skeleton_binding_factory",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "skeleton_service_element_binding_factory_impl",
    srcs = ["skeleton_service_element_binding_factory_impl.cpp"],
    hdrs = ["skeleton_service_element_binding_factory_impl.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    deps = [
        "//score/mw/com/impl:skeleton_base",
        "//score/mw/com/impl/bindings/lola",
        "//score/mw/com/impl/configuration",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "skeleton_event_binding_factory_impl",
    srcs = ["skeleton_event_binding_factory_impl.cpp"],
    hdrs = ["skeleton_event_binding_factory_impl.h"],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = ["//score/mw/com/impl/bindings/lola"],
    tags = ["FFI"],
    deps = [
        ":i_skeleton_event_binding_factory",
        ":skeleton_service_element_binding_factory_impl",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "skeleton_field_binding_factory_impl",
    srcs = ["skeleton_field_binding_factory_impl.cpp"],
    hdrs = ["skeleton_field_binding_factory_impl.h"],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = ["//score/mw/com/impl/bindings/lola"],
    tags = ["FFI"],
    deps = [
        ":i_skeleton_field_binding_factory",
        ":skeleton_service_element_binding_factory_impl",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "proxy_binding_factory",
    srcs = ["proxy_binding_factory.cpp"],
    hdrs = ["proxy_binding_factory.h"],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = [
        "proxy_binding_factory_impl",
    ],
    tags = ["FFI"],
    deps = [
        ":i_proxy_binding_factory",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "proxy_event_binding_factory",
    srcs = ["proxy_event_binding_factory.cpp"],
    hdrs = ["proxy_event_binding_factory.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    deps = [
        "proxy_event_binding_factory_impl",
        ":i_proxy_event_binding_factory",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "proxy_field_binding_factory",
    srcs = ["proxy_field_binding_factory.cpp"],
    hdrs = ["proxy_field_binding_factory.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    deps = [
        "proxy_field_binding_factory_impl",
        ":i_proxy_field_binding_factory",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "skeleton_binding_factory",
    srcs = ["skeleton_binding_factory.cpp"],
    hdrs = ["skeleton_binding_factory.h"],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = [
        "skeleton_binding_factory_impl",
    ],
    tags = ["FFI"],
    deps = [
        ":i_skeleton_binding_factory",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "skeleton_event_binding_factory",
    srcs = ["skeleton_event_binding_factory.cpp"],
    hdrs = ["skeleton_event_binding_factory.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    deps = [
        "skeleton_event_binding_factory_impl",
        ":i_skeleton_event_binding_factory",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "skeleton_field_binding_factory",
    srcs = ["skeleton_field_binding_factory.cpp"],
    hdrs = ["skeleton_field_binding_factory.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    deps = [
        "skeleton_field_binding_factory_impl",
        ":i_skeleton_field_binding_factory",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "proxy_binding_factory_mock",
    testonly = True,
    srcs = ["proxy_binding_factory_mock.cpp"],
    hdrs = ["proxy_binding_factory_mock.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//visibility:public", # platform_only
    ],
    deps = [
        ":i_proxy_binding_factory",
        "@googletest//:gtest",
    ],
)

cc_library(
    name = "proxy_event_binding_factory_mock",
    testonly = True,
    srcs = ["proxy_event_binding_factory_mock.cpp"],
    hdrs = ["proxy_event_binding_factory_mock.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//visibility:public", # platform_only
    ],
    deps = [
        ":i_proxy_event_binding_factory",
        "@googletest//:gtest",
    ],
)

cc_library(
    name = "proxy_field_binding_factory_mock",
    testonly = True,
    srcs = ["proxy_field_binding_factory_mock.cpp"],
    hdrs = ["proxy_field_binding_factory_mock.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//visibility:public", # platform_only
    ],
    deps = [
        ":i_proxy_field_binding_factory",
        "@googletest//:gtest",
    ],
)

cc_library(
    name = "skeleton_binding_factory_mock",
    testonly = True,
    srcs = ["skeleton_binding_factory_mock.cpp"],
    hdrs = ["skeleton_binding_factory_mock.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//visibility:public", # platform_only
    ],
    deps = [
        ":i_skeleton_binding_factory",
        "@googletest//:gtest",
    ],
)

cc_library(
    name = "skeleton_event_binding_factory_mock",
    testonly = True,
    srcs = ["skeleton_event_binding_factory_mock.cpp"],
    hdrs = ["skeleton_event_binding_factory_mock.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//visibility:public", # platform_only
    ],
    deps = [
        ":i_skeleton_event_binding_factory",
        "@googletest//:gtest",
    ],
)

cc_library(
    name = "skeleton_field_binding_factory_mock",
    testonly = True,
    srcs = ["skeleton_field_binding_factory_mock.cpp"],
    hdrs = ["skeleton_field_binding_factory_mock.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//visibility:public", # platform_only
    ],
    deps = [
        ":i_skeleton_field_binding_factory",
        "@googletest//:gtest",
    ],
)

cc_library(
    name = "runtime",
    srcs = [
        "runtime_binding_factory.cpp",
    ],
    hdrs = [
        "runtime_binding_factory.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = [
        "//score/mw/com/impl/bindings/lola/tracing:tracing_runtime",
    ],
    tags = ["FFI"],
    visibility = [
        "//score/mw/com/impl:__subpackages__",
    ],
    deps = [
        "//score/mw/com/impl:runtime_interfaces",
        "//score/mw/com/impl/bindings/lola:runtime",
        "//score/mw/com/impl/configuration",
        "//score/mw/com/impl/tracing/configuration:tracing_filter_config",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "event",
    srcs = [
    ],
    hdrs = [
    ],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com/impl:__subpackages__",
    ],
    deps = [
        ":proxy_event_binding_factory",
        ":sample_allocatee_ptr",
        ":skeleton_event_binding_factory",
        "//score/mw/com/impl:instance_identifier",
        "//score/mw/com/impl:proxy_base",
        "//score/mw/com/impl:skeleton_base",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "field",
    srcs = [
    ],
    hdrs = [
    ],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com/impl:__subpackages__",
    ],
    deps = [
        ":proxy_field_binding_factory",
        ":sample_allocatee_ptr",
        ":skeleton_field_binding_factory",
        "//score/mw/com/impl:instance_identifier",
        "//score/mw/com/impl:proxy_base",
        "//score/mw/com/impl:skeleton_base",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "sample_allocatee_ptr",
    srcs = [
        "sample_allocatee_ptr.cpp",
    ],
    hdrs = [
        "sample_allocatee_ptr.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = ["//score/mw/com/impl:__subpackages__"],
    deps = ["//score/mw/com/impl/bindings/lola:event"],
)

cc_library(
    name = "sample_ptr",
    srcs = [
        "sample_ptr.cpp",
    ],
    hdrs = [
        "sample_ptr.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = ["//score/mw/com/impl:__subpackages__"],
    deps = [
        "//score/mw/com/impl:sample_reference_tracker",
        "//score/mw/com/impl/bindings/lola:event",
        "//score/mw/com/impl/bindings/mock_binding:sample_ptr",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_gtest_unit_test(
    name = "proxy_service_element_binding_factory_test",
    srcs = [
        "proxy_service_element_binding_factory_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":proxy_event_binding_factory",
        ":proxy_field_binding_factory",
        "//score/mw/com/impl/bindings/lola/test:proxy_event_test_resources",
        "//score/mw/com/impl/configuration/test:configuration_store",
        "//score/mw/com/impl/test:dummy_instance_identifier_builder",
    ],
)

cc_gtest_unit_test(
    name = "proxy_event_binding_factory_test",
    srcs = [
        "proxy_event_binding_factory_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":proxy_event_binding_factory",
    ],
)

cc_gtest_unit_test(
    name = "proxy_field_binding_factory_test",
    srcs = [
        "proxy_field_binding_factory_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":proxy_field_binding_factory",
    ],
)

cc_gtest_unit_test(
    name = "skeleton_service_element_binding_factory_test",
    srcs = [
        "skeleton_service_element_binding_factory_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":skeleton_event_binding_factory",
        ":skeleton_field_binding_factory",
        "//score/mw/com/impl/configuration/test:configuration_store",
        "//score/mw/com/impl/test:dummy_instance_identifier_builder",
    ],
)

cc_gtest_unit_test(
    name = "skeleton_event_binding_factory_test",
    srcs = [
        "skeleton_event_binding_factory_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":skeleton_event_binding_factory",
    ],
)

cc_gtest_unit_test(
    name = "skeleton_field_binding_factory_test",
    srcs = [
        "skeleton_field_binding_factory_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":skeleton_field_binding_factory",
    ],
)

cc_gtest_unit_test(
    name = "runtime_binding_factory_test",
    srcs = [
        "runtime_binding_factory_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":runtime_binding_factory",
        "//score/mw/com/impl/bindings/lola/messaging:mock",
        "//score/mw/com/impl/configuration/test:configuration_store",
        "//score/mw/com/impl/test:dummy_instance_identifier_builder",
        "//score/mw/com/message_passing:mock",
    ],
)

cc_test(
    name = "unit_test",
    size = "medium",
    srcs = [
        "proxy_binding_factory_test.cpp",
        "sample_allocatee_ptr_test.cpp",
        "sample_ptr_test.cpp",
        "skeleton_binding_factory_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    tags = ["unit"],
    visibility = [
        "//score/mw/com/impl:__subpackages__",
    ],
    deps = [
        ":event",
        ":plumbing",
        ":proxy_binding_factory_mock",
        "@score-baselibs//score/language/safecpp/coverage_termination_handler",
        "@score-baselibs//score/os/mocklib:fcntl_mock",
        "//score/mw/com/impl:runtime_mock",
        "//score/mw/com/impl/bindings/lola:runtime_mock",
        "//score/mw/com/impl/bindings/lola/messaging:mock",
        "//score/mw/com/impl/bindings/lola/test:proxy_event_test_resources",
        "//score/mw/com/impl/bindings/lola/test_doubles",
        "//score/mw/com/impl/bindings/mock_binding",
        "//score/mw/com/impl/test:dummy_instance_identifier_builder",
        "//score/mw/com/message_passing:mock",
        "@score-baselibs//score/mw/log",
        "@googletest//:gtest_main",
    ],
)

cc_unit_test_suites_for_host_and_qnx(
    name = "unit_test_suite",
    cc_unit_tests = [
        ":proxy_event_binding_factory_test",
        ":proxy_field_binding_factory_test",
        ":proxy_service_element_binding_factory_test",
        ":runtime_binding_factory_test",
        ":skeleton_event_binding_factory_test",
        ":skeleton_field_binding_factory_test",
        ":skeleton_service_element_binding_factory_test",
        ":unit_test",
    ],
    visibility = [
        "//score/mw/com/impl:__subpackages__",
    ],
)

rust_library(
    name = "sample_ptr_rs",
    srcs = ["sample_ptr.rs"],
    visibility = [
        "//score/mw/com:__subpackages__",
    ],
    deps = ["@crate_index//:libc"],
)
