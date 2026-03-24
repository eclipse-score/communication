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
load("@score_baselibs//score/language/safecpp:toolchain_features.bzl", "COMPILER_WARNING_FEATURES")

# =============================================================================
# Aliases — these targets moved to new packages but kept here for compatibility
# =============================================================================

cc_library(
    name = "atomic_interface",
    hdrs = ["i_atomic.h"],
    deprecation = "Use @score_baselibs//score/concurrency/atomic:atomic_interface directly. This forwarding header will be removed.",
    visibility = ["//visibility:public"],
    deps = ["@score_baselibs//score/concurrency/atomic:atomic_interface"],
)

cc_library(
    name = "atomic_indirector",
    hdrs = ["atomic_indirector.h"],
    deprecation = "Use @score_baselibs//score/concurrency/atomic:atomic_indirector directly. This forwarding header will be removed.",
    visibility = ["//visibility:public"],
    deps = ["@score_baselibs//score/concurrency/atomic:atomic_indirector"],
)

cc_library(
    name = "atomic_indirector_mock_binding",
    hdrs = ["atomic_mock.h"],
    deprecation = "Use @score_baselibs//score/concurrency/atomic:atomic_indirector_mock_binding directly. This forwarding header will be removed.",
    testonly = True,
    visibility = ["//visibility:public"],
    deps = ["@score_baselibs//score/concurrency/atomic:atomic_indirector_mock_binding"],
)

# =============================================================================
# Primitives — these stay in baselibs
# =============================================================================

cc_library(
    name = "pointer_arithmetic_util",
    srcs = ["pointer_arithmetic_util.cpp"],
    hdrs = ["pointer_arithmetic_util.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//visibility:public",
    ],
    deps = [
        "@score_baselibs//score/language/safecpp/safe_math",
        "@score_baselibs//score/memory:data_type_size_info",
        "@score_baselibs//score/mw/log:frontend",
    ],
)

cc_library(
    name = "allocation_algorithm",
    srcs = ["allocation_algorithm.cpp"],
    hdrs = ["allocation_algorithm.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//visibility:public",
    ],
    deps = [
        ":pointer_arithmetic_util",
    ],
)

cc_library(
    name = "managed_memory_resource",
    srcs = ["managed_memory_resource.cpp"],
    hdrs = ["managed_memory_resource.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//visibility:public",
    ],
    deps = ["@score_baselibs//score/language/futurecpp"],
)

cc_library(
    name = "user_permission",
    hdrs = ["user_permission.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = ["//visibility:public"],
    deps = ["@score_baselibs//score/os/utils/acl"],
)

cc_library(
    name = "i_shared_memory_resource",
    srcs = ["i_shared_memory_resource.cpp"],
    hdrs = ["i_shared_memory_resource.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//visibility:public",
    ],
    deps = [
        ":managed_memory_resource",
        ":user_permission",
        "@score_baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "memory_region_bounds",
    srcs = ["memory_region_bounds.cpp"],
    hdrs = ["memory_region_bounds.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//visibility:public",
    ],
    deps = [
        "@score_baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "memory_region_map",
    srcs = ["memory_region_map.cpp"],
    hdrs = ["memory_region_map.h"],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = [
        "@score_baselibs//score/mw/log:frontend",
    ],
    tags = ["FFI"],
    visibility = [
        "//visibility:public",
    ],
    deps = [
        "@score_baselibs//score/concurrency/atomic:atomic_indirector",
        ":memory_region_bounds",
        "@score_baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "memory_resource_registry",
    srcs = ["memory_resource_registry.cpp"],
    hdrs = ["memory_resource_registry.h"],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = [
        ":pointer_arithmetic_util",
        ":shared_memory_error",
        "@score_baselibs//score/mw/log:frontend",
    ],
    tags = ["FFI"],
    visibility = [
        "//visibility:public",
    ],
    deps = [
        ":managed_memory_resource",
        ":memory_region_bounds",
        ":memory_region_map",
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/result",
    ],
)

cc_library(
    name = "memory_resource_proxy",
    srcs = ["memory_resource_proxy.cpp"],
    hdrs = ["memory_resource_proxy.h"],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = [
        ":memory_region_bounds",
        ":memory_resource_registry",
        ":pointer_arithmetic_util",
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/mw/log:frontend",
    ],
    tags = ["FFI"],
    visibility = [
        "//visibility:public",
    ],
)

cc_library(
    name = "offset_ptr",
    srcs = [
        "offset_ptr.cpp",
    ],
    hdrs = [
        "offset_ptr.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//visibility:public",
    ],
    deps = [
        ":memory_region_bounds",
        ":memory_resource_registry",
        ":offset_ptr_bounds_check",
        ":pointer_arithmetic_util",
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/language/safecpp/safe_math",
        "@score_baselibs//score/quality/compiler_warnings",
    ],
)

cc_library(
    name = "offset_ptr_bounds_check",
    srcs = [
        "offset_ptr_bounds_check.cpp",
    ],
    hdrs = [
        "offset_ptr_bounds_check.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = [
        ":pointer_arithmetic_util",
        "@score_baselibs//score/mw/log:frontend",
    ],
    tags = ["FFI"],
    visibility = [
        "//visibility:public",
    ],
    deps = [
        ":memory_region_map",
        ":memory_resource_registry",
        "@score_baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "polymorphic_offset_ptr_allocator",
    srcs = ["polymorphic_offset_ptr_allocator.cpp"],
    hdrs = ["polymorphic_offset_ptr_allocator.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//visibility:public",
    ],
    deps = [
        ":memory_resource_proxy",
        ":offset_ptr",
        "@score_baselibs//score/language/safecpp/safe_math",
    ],
)

cc_library(
    name = "map",
    srcs = ["map.cpp"],
    hdrs = ["map.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//visibility:public",
    ],
    deps = [
        ":polymorphic_offset_ptr_allocator",
    ] + select({
        "@platforms//os:linux": [
            "@boost.container",
            "@boost.interprocess",
        ],
        "@platforms//os:qnx": [],
    }),
)

cc_library(
    name = "string",
    srcs = ["string.cpp"],
    hdrs = ["string.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//visibility:public",
    ],
    deps = [
        ":polymorphic_offset_ptr_allocator",
        "@score_baselibs//score/language/safecpp/string_view:char_traits_wrapper",
    ],
)

cc_library(
    name = "vector",
    srcs = ["vector.cpp"],
    hdrs = ["vector.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//visibility:public",
    ],
    deps = [
        ":polymorphic_offset_ptr_allocator",
    ],
)

cc_library(
    name = "shared_memory_error",
    srcs = ["shared_memory_error.cpp"],
    hdrs = ["shared_memory_error.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = ["//visibility:public"],
    deps = [
        "@score_baselibs//score/result",
    ],
)

cc_library(
    name = "types",
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//visibility:public",
    ],
    deps = [
        ":managed_memory_resource",
        ":map",
        ":memory_region_map",
        ":memory_resource_proxy",
        ":memory_resource_registry",
        ":offset_ptr",
        ":polymorphic_offset_ptr_allocator",
        ":shared_memory_error",
        ":string",
        ":vector",
    ],
)

# =============================================================================
# Tests for primitives
# =============================================================================

cc_unit_test(
    name = "pointer_arithmetic_util_precondition_violation_test",
    srcs = [
        "pointer_arithmetic_util_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES + [
        "-aborts_upon_exception",
    ],
    visibility = [
        "@score_baselibs//score/memory:__pkg__",
    ],
    deps = [
        ":pointer_arithmetic_util",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
        "@score_baselibs//score/language/futurecpp:futurecpp_test_support",
        "@score_baselibs//score/language/safecpp/coverage_termination_handler",
        "@score_baselibs//score/mw/log:backend_stub_testutil",
    ],
)

cc_unit_test(
    name = "pointer_arithmetic_util_calculate_aligned_size_test",
    srcs = [
        "pointer_arithmetic_util_calculate_aligned_size_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES + [
        "-aborts_upon_exception",
    ],
    visibility = [
        "@score_baselibs//score/memory:__pkg__",
    ],
    deps = [
        ":pointer_arithmetic_util",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
        "@score_baselibs//score/language/futurecpp:futurecpp_test_support",
        "@score_baselibs//score/mw/log:backend_stub_testutil",
    ],
)

cc_gtest_unit_test(
    name = "memory_region_bounds_test",
    srcs = [
        "memory_region_bounds_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "@score_baselibs//score/memory:__pkg__",
    ],
    deps = [
        ":memory_region_bounds",
    ],
)

cc_gtest_unit_test(
    name = "memory_resource_registry_test",
    srcs = [
        "memory_resource_registry_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "@score_baselibs//score/memory:__pkg__",
    ],
    deps = [
        ":memory_resource_registry",
        ":pointer_arithmetic_util",
        ":shared_memory_error",
        "@score_baselibs//score/mw/log:backend_stub_testutil",
    ],
)


cc_gtest_unit_test(
    name = "map_test",
    srcs = [
        "map_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "@score_baselibs//score/memory:__pkg__",
    ],
    deps = [
        ":map",
        ":vector",
        "@score_baselibs//score/memory/shared/fake:fake_memory_resources",
        "@score_baselibs//score/mw/log:backend_stub_testutil",
    ],
)

cc_gtest_unit_test(
    name = "memory_region_map_test",
    srcs = [
        "memory_region_map_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "@score_baselibs//score/memory:__pkg__",
    ],
    deps = [
        ":memory_region_map",
        "@score_baselibs//score/concurrency/atomic:atomic_indirector_mock_binding",
        "@score_baselibs//score/mw/log:backend_stub_testutil",
    ],
)

cc_gtest_unit_test(
    name = "polymorphic_offset_ptr_allocator_test",
    srcs = [
        "polymorphic_offset_ptr_allocator_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "@score_baselibs//score/memory:__pkg__",
    ],
    deps = [
        ":polymorphic_offset_ptr_allocator",
        "@score_baselibs//score/memory/shared/fake:fake_memory_resources",
        "@score_baselibs//score/mw/log:backend_stub_testutil",
    ],
)

cc_gtest_unit_test(
    name = "shared_memory_error_test",
    srcs = [
        "shared_memory_error_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "@score_baselibs//score/memory:__pkg__",
    ],
    deps = [
        ":shared_memory_error",
    ],
)

cc_gtest_unit_test(
    name = "vector_test",
    srcs = [
        "vector_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "@score_baselibs//score/memory:__pkg__",
    ],
    deps = [
        ":vector",
        "@score_baselibs//score/memory/shared/fake:fake_memory_resources",
        "@score_baselibs//score/mw/log:backend_stub_testutil",
    ],
)

cc_gtest_unit_test(
    name = "string_test",
    srcs = [
        "string_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES,
    target_compatible_with = [
        "@platforms//os:linux",
    ],
    visibility = [
        "@score_baselibs//score/memory:__pkg__",
    ],
    deps = [
        ":string",
        "@score_baselibs//score/memory/shared/fake:fake_memory_resources",
        "@score_baselibs//score/mw/log:backend_stub_testutil",
    ],
)

# Separate test suite for string test as it should not be executed on qnx
test_suite(
    name = "unit_tests_host",
    tests = [
        ":string_test",
    ],
    visibility = [
        "@score_baselibs//score/memory:__pkg__",
    ],
)

cc_unit_test_suites_for_host_and_qnx(
    name = "unit_test_suite",
    cc_unit_tests = [
        ":map_test",
        ":memory_region_bounds_test",
        ":memory_region_map_test",
        ":memory_resource_registry_test",
        ":polymorphic_offset_ptr_allocator_test",
        ":pointer_arithmetic_util_precondition_violation_test",
        ":pointer_arithmetic_util_calculate_aligned_size_test",
        ":shared_memory_error_test",
        ":vector_test",
    ],
    test_suites_from_sub_packages = [
        "@score_baselibs//score/memory/shared/test_offset_ptr:unit_test_suite",
    ],
    visibility = [
        "@score_baselibs//score/memory:__pkg__",
    ],
)
