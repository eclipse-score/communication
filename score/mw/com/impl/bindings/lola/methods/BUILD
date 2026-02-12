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

load("@rules_cc//cc:defs.bzl", "cc_library")
load("@score_baselibs//:bazel/unit_tests.bzl", "cc_gtest_unit_test", "cc_unit_test", "cc_unit_test_suites_for_host_and_qnx")
load("//score/mw:common_features.bzl", "COMPILER_WARNING_FEATURES")

cc_library(
    name = "proxy_instance_identifier",
    srcs = ["proxy_instance_identifier.cpp"],
    hdrs = ["proxy_instance_identifier.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com/impl/bindings/lola:__pkg__",
        "//score/mw/com/impl/bindings/lola/messaging:__pkg__",
    ],
    deps = [
        "//score/mw/com/impl/configuration",
        "@score_logging//score/mw/log",
    ],
)

cc_library(
    name = "proxy_method_instance_identifier",
    srcs = ["proxy_method_instance_identifier.cpp"],
    hdrs = ["proxy_method_instance_identifier.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com/impl/bindings/lola:__pkg__",
        "//score/mw/com/impl/bindings/lola/messaging:__pkg__",
    ],
    deps = [
        ":proxy_instance_identifier",
        "//score/mw/com/impl/configuration",
        "@score_logging//score/mw/log",
    ],
)

cc_library(
    name = "skeleton_instance_identifier",
    srcs = ["skeleton_instance_identifier.cpp"],
    hdrs = ["skeleton_instance_identifier.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com/impl/bindings/lola:__pkg__",
        "//score/mw/com/impl/bindings/lola/messaging:__pkg__",
    ],
    deps = [
        "//score/mw/com/impl/configuration",
        "@score_logging//score/mw/log",
    ],
)

cc_library(
    name = "method_data",
    srcs = [
        "method_data.cpp",
    ],
    hdrs = [
        "method_data.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = ["//score/mw/com/impl/bindings/lola:__pkg__"],
    deps = [
        ":type_erased_call_queue",
        "//score/mw/com/impl/configuration",
        "@score_baselibs//score/containers:dynamic_array",
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/memory/shared:managed_memory_resource",
        "@score_baselibs//score/memory/shared:polymorphic_offset_ptr_allocator",
    ],
)

cc_library(
    name = "method_resource_map",
    srcs = [
        "method_resource_map.cpp",
    ],
    hdrs = [
        "method_resource_map.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = [
        "@score_baselibs//score/language/futurecpp",
        "@score_logging//score/mw/log",
    ],
    tags = ["FFI"],
    visibility = ["//score/mw/com/impl/bindings/lola:__pkg__"],
    deps = [
        ":proxy_instance_identifier",
        "//score/mw/com/impl/configuration",
        "@score_baselibs//score/memory/shared",
    ],
)

cc_library(
    name = "type_erased_call_queue",
    srcs = [
        "type_erased_call_queue.cpp",
    ],
    hdrs = [
        "type_erased_call_queue.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = [
        "@score_baselibs//score/memory/shared:pointer_arithmetic_util",
    ],
    tags = ["FFI"],
    visibility = ["//score/mw/com/impl/bindings/lola:__pkg__"],
    deps = [
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/memory:data_type_size_info",
        "@score_baselibs//score/memory/shared:memory_resource_proxy",
        "@score_baselibs//score/memory/shared:offset_ptr",
    ],
)

cc_library(
    name = "method_error",
    srcs = ["method_error.cpp"],
    hdrs = ["method_error.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com/impl:__subpackages__",
    ],
    deps = ["@score_baselibs//score/result"],
)

cc_library(
    name = "offered_state_machine",
    srcs = ["offered_state_machine.cpp"],
    hdrs = ["offered_state_machine.h"],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = ["@score_baselibs//score/language/futurecpp"],
    tags = ["FFI"],
    visibility = ["//score/mw/com/impl/bindings/lola:__pkg__"],
)

cc_unit_test(
    name = "type_erased_call_queue_test",
    srcs = [
        "type_erased_call_queue_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES + [
        # These tests catch exceptions instead of using gtest EXPECT_DEATH so we disable aborts_upon_exception.
        "-aborts_upon_exception",
    ],
    deps = [
        ":type_erased_call_queue",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
        "@score_baselibs//score/language/futurecpp:futurecpp_test_support",
        "@score_baselibs//score/language/safecpp/coverage_termination_handler",
        "@score_baselibs//score/memory:data_type_size_info",
        "@score_baselibs//score/memory/shared:memory_resource_proxy",
        "@score_baselibs//score/memory/shared:pointer_arithmetic_util",
        "@score_baselibs//score/memory/shared/fake:fake_memory_resources",
        "@score_logging//score/mw/log",
    ],
)

cc_gtest_unit_test(
    name = "proxy_instance_identifier_test",
    srcs = ["proxy_instance_identifier_test.cpp"],
    deps = [
        ":proxy_instance_identifier",
    ],
)

cc_gtest_unit_test(
    name = "proxy_method_instance_identifier_test",
    srcs = ["proxy_method_instance_identifier_test.cpp"],
    deps = [
        ":proxy_method_instance_identifier",
    ],
)

cc_gtest_unit_test(
    name = "skeleton_instance_identifier_test",
    srcs = ["skeleton_instance_identifier_test.cpp"],
    deps = [
        ":skeleton_instance_identifier",
    ],
)

cc_gtest_unit_test(
    name = "method_data_test",
    srcs = [
        "method_data_test.cpp",
    ],
    deps = [
        ":method_data",
        "@score_baselibs//score/memory/shared/fake:fake_memory_resources",
    ],
)

cc_gtest_unit_test(
    name = "method_error_test",
    srcs = [
        "method_error_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES + [
        # These tests catch exceptions instead of using gtest EXPECT_DEATH so we disable aborts_upon_exception.
        "-aborts_upon_exception",
    ],
    deps = [
        ":method_error",
        "@score_baselibs//score/language/futurecpp:futurecpp_test_support",
    ],
)

cc_gtest_unit_test(
    name = "method_resource_map_test",
    srcs = [
        "method_resource_map_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES + [
        # These tests catch exceptions instead of using gtest EXPECT_DEATH so we disable aborts_upon_exception.
        "-aborts_upon_exception",
    ],
    deps = [
        ":method_resource_map",
        "@score_baselibs//score/language/futurecpp:futurecpp_test_support",
        "@score_baselibs//score/memory/shared:shared_memory_resource_mock",
    ],
)

cc_unit_test(
    name = "offered_state_machine_test",
    srcs = ["offered_state_machine_test.cpp"],
    features = COMPILER_WARNING_FEATURES + [
        # These tests catch exceptions instead of using gtest EXPECT_DEATH so we disable aborts_upon_exception.
        "-aborts_upon_exception",
    ],
    deps = [
        ":offered_state_machine",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
        "@score_baselibs//score/language/futurecpp:futurecpp_test_support",
    ],
)

cc_unit_test_suites_for_host_and_qnx(
    name = "unit_test_suite",
    cc_unit_tests = [
        ":proxy_instance_identifier_test",
        ":proxy_method_instance_identifier_test",
        ":skeleton_instance_identifier_test",
        ":type_erased_call_queue_test",
        ":method_data_test",
        ":method_error_test",
        ":method_resource_map_test",
        ":offered_state_machine_test",
    ],
    visibility = [
        "//score/mw/com/impl/bindings/lola:__pkg__",
    ],
)
