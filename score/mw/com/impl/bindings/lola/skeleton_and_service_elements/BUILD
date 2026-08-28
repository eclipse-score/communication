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
load("@score_tooling//bazel/rules/rules_score:rules_score.bzl", "component", "unit")
load("//quality/unit_testing:unit_testing.bzl", "cc_unit_test")
load("//score/mw:common_features.bzl", "COMPILER_WARNING_FEATURES")

cc_library(
    name = "skeleton",
    srcs = [
        "skeleton.cpp",
        "skeleton_event.cpp",
        "skeleton_event_common.cpp",
        "skeleton_event_properties.cpp",
        "skeleton_memory_manager.cpp",
        "skeleton_method.cpp",
    ],
    hdrs = [
        "skeleton.h",
        "skeleton_event.h",
        "skeleton_event_common.h",
        "skeleton_event_properties.h",
        "skeleton_memory_manager.h",
        "skeleton_method.h",
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
        "//score/memory/shared",
        "//score/memory/shared:i_shared_memory_resource",
        "//score/memory/shared:lock_file",
        "//score/memory/shared:new_delete_delegate_resource",
        "//score/memory/shared/flock:exclusive_flock_mutex",
        "//score/memory/shared/flock:flock_mutex_and_lock",
        "//score/mw/com/impl:error",
        "//score/mw/com/impl:generic_skeleton_event_binding",
        "//score/mw/com/impl:instance_identifier",
        "//score/mw/com/impl:runtime",
        "//score/mw/com/impl:skeleton_binding",
        "//score/mw/com/impl:skeleton_event_binding",
        "//score/mw/com/impl/bindings/lola:event",
        "//score/mw/com/impl/bindings/lola:i_partial_restart_path_builder",
        "//score/mw/com/impl/bindings/lola:i_shm_path_builder",
        "//score/mw/com/impl/bindings/lola:partial_restart_path_builder",
        "//score/mw/com/impl/bindings/lola:proxy_instance_identifier",
        "//score/mw/com/impl/bindings/lola:service_data_control",
        "//score/mw/com/impl/bindings/lola:service_data_storage",
        "//score/mw/com/impl/bindings/lola:shm_path_builder",
        "//score/mw/com/impl/bindings/lola:skeleton_instance_identifier",
        "//score/mw/com/impl/bindings/lola:type_erased_sample_ptrs_guard",
        "//score/mw/com/impl/bindings/lola/methods:method_data",
        "//score/mw/com/impl/bindings/lola/methods:method_resource_map",
        "//score/mw/com/impl/bindings/lola/methods:type_erased_call_queue",
        "//score/mw/com/impl/configuration",
        "//score/mw/com/impl/methods:skeleton_method_binding",
        "//score/mw/com/impl/plumbing:sample_allocatee_ptr",
        "//score/mw/com/impl/plumbing:sample_ptr",
        "//score/mw/com/impl/tracing:skeleton_event_tracing",
        "//score/mw/com/impl/util:arithmetic_utils",
        "@score_baselibs//score/filesystem",
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/language/safecpp/safe_math",
        "@score_baselibs//score/language/safecpp/scoped_function:scope",
        "@score_baselibs//score/memory:data_type_size_info",
        "@score_baselibs//score/result",
    ],
)

cc_unit_test(
    name = "skeleton_method_test",
    srcs = ["skeleton_method_test.cpp"],
    features = COMPILER_WARNING_FEATURES + [
        # These tests catch exceptions instead of using gtest EXPECT_DEATH so we disable aborts_upon_exception.
        "-aborts_upon_exception",
    ],
    deps = [
        ":skeleton",
        "//score/mw/com/impl/bindings/lola/test:skeleton_test_resources",
        "//score/mw/com/impl/configuration/test:configuration_store",
        "@googletest//:gtest",
        "@score_baselibs//score/language/futurecpp:futurecpp_test_support",
    ],
)

cc_unit_test(
    name = "skeleton_test",
    srcs = ["skeleton_test.cpp"],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    deps = [
        ":skeleton",
        "//score/mw/com/impl/bindings/lola/test:skeleton_test_resources",
        "//score/mw/com/impl/bindings/lola/test:transaction_log_test_resources",
        "//score/mw/com/impl/bindings/mock_binding",
        "//score/mw/com/impl/tracing:tracing_runtime_mock",
    ],
)

cc_unit_test(
    name = "skeleton_method_handling_test",
    srcs = [
        "skeleton_method_handling_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES + [
        # These tests catch exceptions instead of using gtest EXPECT_DEATH so we disable aborts_upon_exception.
        "-aborts_upon_exception",
    ],
    deps = [
        ":skeleton",
        "//score/memory/shared:shared_memory_resource_mock",
        "//score/memory/shared/fake:my_bounded_shared_memory_resource",
        "//score/mw/com/impl:binding_type",
        "//score/mw/com/impl/bindings/lola/test:skeleton_test_resources",
        "//score/mw/com/impl/configuration/test:configuration_store",
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/language/futurecpp:futurecpp_test_support",
        "@score_baselibs//score/memory:data_type_size_info",
        "@score_baselibs//score/result",
    ],
)

cc_unit_test(
    name = "skeleton_event_common_test",
    srcs = ["skeleton_event_common_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":skeleton",
        "//score/mw/com/impl/bindings/lola/test:skeleton_event_test_resources",
        "//score/mw/com/impl/bindings/lola/test:transaction_log_test_resources",
    ],
)

cc_unit_test(
    name = "skeleton_event_test",
    srcs = [
        "skeleton_event_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES + [
        # These tests catch exceptions instead of using gtest EXPECT_DEATH so we disable aborts_upon_exception.
        "-aborts_upon_exception",
    ],
    deps = [
        "//score/mw/com/impl/bindings/lola/test:skeleton_event_test_resources",
        "@score_baselibs//score/language/futurecpp:futurecpp_test_support",
    ],
)

cc_unit_test(
    name = "skeleton_event_tracing_test",
    srcs = [
        "skeleton_event_tracing_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        "//score/mw/com/impl/bindings/lola/test:skeleton_event_test_resources",
        "//score/mw/com/impl/bindings/lola/test:transaction_log_test_resources",
        "//score/mw/com/impl/tracing/configuration:tracing_filter_config_mock",
    ],
)
