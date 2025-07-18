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
load("@score-baselibs//:bazel/unit_tests.bzl", "cc_gtest_unit_test", "cc_unit_test", "cc_unit_test_suites_for_host_and_qnx")
load("@score-baselibs//score/language/safecpp:toolchain_features.bzl", "COMPILER_WARNING_FEATURES")

cc_library(
    name = "atomic_interface",
    hdrs = ["i_atomic.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "@score-baselibs//score/memory/shared:__pkg__",
    ],
    deps = [],
)

cc_library(
    name = "atomic_indirector",
    srcs = ["atomic_indirector.cpp"],
    hdrs = ["atomic_indirector.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//visibility:public", # platform_only
    ],
    deps = [":atomic_interface"],
)

cc_library(
    name = "atomic_indirector_mock_binding",
    testonly = True,
    srcs = ["atomic_mock.cpp"],
    hdrs = ["atomic_mock.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//visibility:public"],
    deps = [
        ":atomic_indirector",
        "@googletest//:gtest",
    ],
)

cc_library(
    name = "pointer_arithmetic_util",
    srcs = ["pointer_arithmetic_util.cpp"],
    hdrs = ["pointer_arithmetic_util.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//visibility:public", # platform_only
    ],
    deps = [
        "@score-baselibs//score/mw/log:frontend",
    ],
)

cc_library(
    name = "i_shared_memory_resource",
    srcs = ["i_shared_memory_resource.cpp"],
    hdrs = ["i_shared_memory_resource.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//visibility:public", # platform_only
    ],
    deps = [
        ":types",
        ":user_permission",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "shared_memory_resource_heap_allocator_mock",
    testonly = True,
    srcs = ["shared_memory_resource_heap_allocator_mock.cpp"],
    hdrs = ["shared_memory_resource_heap_allocator_mock.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//visibility:public", # platform_only
    ],
    deps = [
        ":i_shared_memory_resource",
        ":new_delete_delegate_resource",
        "@googletest//:gtest",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "shared_memory_resource_mock",
    testonly = True,
    srcs = ["shared_memory_resource_mock.cpp"],
    hdrs = ["shared_memory_resource_mock.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//visibility:public", # platform_only
    ],
    deps = [
        ":i_shared_memory_resource",
        "@googletest//:gtest",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "shared_memory_resource",
    srcs = ["shared_memory_resource.cpp"],
    hdrs = ["shared_memory_resource.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    deps = [
        ":i_shared_memory_resource",
        ":lock_file",
        ":pointer_arithmetic_util",
        ":types",
        "@score-baselibs//score/language/safecpp/safe_math",
        "@score-baselibs//score/memory/shared/sealedshm/sealedshm_wrapper:sealedshm",
        "@score-baselibs//score/memory/shared/typedshm/typedshm_wrapper:typedmemory",
        "@score-baselibs//score/os:errno_logging",
        "@score-baselibs//score/os:fcntl",
        "@score-baselibs//score/os:mman",
        "@score-baselibs//score/os:stat",
        "@score-baselibs//score/os:unistd",
        "@score-baselibs//score/os/utils/acl",
        "@score-baselibs//score/os/utils/interprocess:interprocess_mutex",
        "@score-baselibs//score/mw/log:frontend",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "i_shared_memory_factory",
    srcs = ["i_shared_memory_factory.cpp"],
    hdrs = ["i_shared_memory_factory.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    deps = [
        ":i_shared_memory_resource",
        ":types",
        "@score-baselibs//score/memory/shared/typedshm/typedshm_wrapper:typedmemory",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "shared_memory_factory_mock",
    testonly = True,
    srcs = ["shared_memory_factory_mock.cpp"],
    hdrs = ["shared_memory_factory_mock.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//visibility:public", # platform_only
    ],
    deps = [
        ":i_shared_memory_factory",
        ":types",
        "@score-baselibs//score/memory/shared/typedshm/typedshm_wrapper:typedmemory",
        "@googletest//:gtest",
    ],
)

cc_library(
    name = "shared_memory_factory_impl",
    srcs = ["shared_memory_factory_impl.cpp"],
    hdrs = ["shared_memory_factory_impl.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    deps = [
        ":i_shared_memory_factory",
        ":shared_memory_resource",
        ":types",
        "@score-baselibs//score/memory/shared/typedshm/typedshm_wrapper:typedmemory",
        "@score-baselibs//score/os:mman",
        "@score-baselibs//score/os:unistd",
        "@score-baselibs//score/os/utils/acl",
        "@score-baselibs//score/mw/log:frontend",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "shared_memory_factory",
    srcs = ["shared_memory_factory.cpp"],
    hdrs = ["shared_memory_factory.h"],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = [
        "shared_memory_factory_impl",
        "@score-baselibs//score/os:errno_logging",
        "@score-baselibs//score/os:unistd",
        "@score-baselibs//score/mw/log:frontend",
    ],
    tags = ["FFI"],
    deps = [
        ":i_shared_memory_factory",
        ":i_shared_memory_resource",
        ":types",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "shared",
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//visibility:public",
    ],
    deps = [
        ":shared_memory_factory",
        ":shared_memory_resource",
    ],
)

cc_library(
    name = "lock_file",
    srcs = ["lock_file.cpp"],
    hdrs = ["lock_file.h"],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = [
        "@score-baselibs//score/os:fcntl",
        "@score-baselibs//score/os:stat",
        "@score-baselibs//score/os:unistd",
    ],
    tags = ["FFI"],
    visibility = [
        "//visibility:public", # platform_only
    ],
    deps = [
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "managed_memory_resource",
    srcs = ["managed_memory_resource.cpp"],
    hdrs = ["managed_memory_resource.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//visibility:public", # platform_only
    ],
    deps = ["@score-baselibs//score/language/futurecpp"],
)

cc_library(
    name = "memory_region_map",
    srcs = ["memory_region_map.cpp"],
    hdrs = ["memory_region_map.h"],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = [
        "@score-baselibs//score/mw/log:frontend",
    ],
    tags = ["FFI"],
    visibility = [
        "//visibility:public", # platform_only
    ],
    deps = [
        ":atomic_indirector",
        ":memory_region_bounds",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "memory_region_bounds",
    srcs = ["memory_region_bounds.cpp"],
    hdrs = ["memory_region_bounds.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    deps = [
        "@score-baselibs//score/language/futurecpp",
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
        "@score-baselibs//score/mw/log:frontend",
    ],
    tags = ["FFI"],
    visibility = [
        "//visibility:public", # platform_only
    ],
    deps = [
        ":managed_memory_resource",
        ":memory_region_bounds",
        ":memory_region_map",
        "@score-baselibs//score/result",
        "@score-baselibs//score/language/futurecpp",
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
        "@score-baselibs//score/mw/log:frontend",
        "@score-baselibs//score/language/futurecpp",
    ],
    tags = ["FFI"],
    visibility = [
        "//visibility:public", # platform_only
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
        "//visibility:public", # platform_only
    ],
    deps = [
        ":memory_region_bounds",
        ":memory_resource_registry",
        ":offset_ptr_bounds_check",
        ":pointer_arithmetic_util",
        "@score-baselibs//score/language/safecpp/safe_math",
        "@score-baselibs//score/language/futurecpp",
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
        "@score-baselibs//score/mw/log:frontend",
    ],
    tags = ["FFI"],
    visibility = [
        "//visibility:public", # platform_only
    ],
    deps = [
        ":memory_region_map",
        ":memory_resource_registry",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "polymorphic_offset_ptr_allocator",
    srcs = ["polymorphic_offset_ptr_allocator.cpp"],
    hdrs = ["polymorphic_offset_ptr_allocator.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//visibility:public", # platform_only
    ],
    deps = [
        ":memory_resource_proxy",
        ":offset_ptr",
        "@score-baselibs//score/language/safecpp/safe_math",
    ],
)

cc_library(
    name = "map",
    srcs = ["map.cpp"],
    hdrs = ["map.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//visibility:public", # platform_only
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
        "//visibility:public", # platform_only
    ],
    deps = [
        ":polymorphic_offset_ptr_allocator",
    ],
)

cc_library(
    name = "vector",
    srcs = ["vector.cpp"],
    hdrs = ["vector.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//visibility:public", # platform_only
    ],
    deps = [
        ":polymorphic_offset_ptr_allocator",
    ],
)

cc_library(
    name = "types",
    srcs = [
    ],
    hdrs = [
    ],
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

cc_library(
    name = "shared_memory_test_resources",
    testonly = True,
    srcs = [
        "shared_memory_test_resources.cpp",
    ],
    hdrs = [
        "shared_memory_test_resources.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = ["@score-baselibs//score/memory/shared:__subpackages__"],
    deps = [
        ":shared",
        "@score-baselibs//score/memory/shared/sealedshm/sealedshm_wrapper:sealedshm_mock",
        "@score-baselibs//score/memory/shared/typedshm/typedshm_wrapper/test:typedmemory_mock",
        "@score-baselibs//score/os/mocklib:acl_mock",
        "@score-baselibs//score/os/mocklib:fcntl_mock",
        "@score-baselibs//score/os/mocklib:mman_mock",
        "@score-baselibs//score/os/mocklib:stat_mock",
        "@score-baselibs//score/os/mocklib:unistd_mock",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "user_permission",
    hdrs = ["user_permission.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = ["@score-baselibs//score/memory/shared:__subpackages__"],
    deps = ["@score-baselibs//score/os/utils/acl"],
)

cc_library(
    name = "new_delete_delegate_resource",
    srcs = ["new_delete_delegate_resource.cpp"],
    hdrs = ["new_delete_delegate_resource.h"],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = [
        "@score-baselibs//score/language/safecpp/safe_math",
        "@score-baselibs//score/mw/log",
    ],
    tags = ["FFI"],
    visibility = [
        "//visibility:public",
    ],
    deps = [
        ":pointer_arithmetic_util",
        ":shared",
        ":types",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "shared_memory_error",
    srcs = ["shared_memory_error.cpp"],
    hdrs = ["shared_memory_error.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = ["@score-baselibs//score/memory/shared:__pkg__"],
    deps = [
        "@score-baselibs//score/result",
    ],
)

cc_library(
    name = "fake_memory_resources",
    testonly = True,
    srcs = [
        "fake/my_bounded_memory_resource.cpp",
        "fake/my_memory_resource.cpp",
    ],
    hdrs = [
        "fake/my_bounded_memory_resource.h",
        "fake/my_memory_resource.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//visibility:public"],
    deps = [
        ":types",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_unit_test(
    name = "pointer_arithmetic_util_precondition_violation_test",
    srcs = [
        "pointer_arithmetic_util_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES + [
        # These tests catch exceptions instead of using gtest EXPECT_DEATH so we disable aborts_upon_exception.
        "-aborts_upon_exception",
    ],
    visibility = [
        "@score-baselibs//score/memory:__pkg__",
    ],
    deps = [
        ":pointer_arithmetic_util",
        "@score-baselibs//score/language/safecpp/coverage_termination_handler",  # TODO: Remove when SubtractPointersBytes is no longer noexcept and we remove the last death test.
        "@score-baselibs//score/mw/log",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
        "@score-baselibs//score/language/futurecpp:futurecpp_test_support",
    ],
)

cc_gtest_unit_test(
    name = "shared_memory_factory_test",
    srcs = [
        "shared_memory_factory_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "@score-baselibs//score/memory:__pkg__",
    ],
    deps = [
        ":shared_memory_factory",
        ":shared_memory_test_resources",
        "@score-baselibs//score/mw/log",
    ],
)

cc_gtest_unit_test(
    name = "memory_region_bounds_test",
    srcs = [
        "memory_region_bounds_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "@score-baselibs//score/memory:__pkg__",
    ],
    deps = [
        ":memory_region_bounds",
    ],
)

cc_gtest_unit_test(
    name = "atomic_indirector_test",
    srcs = ["atomic_indirector_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "@score-baselibs//score/memory:__pkg__",
    ],
    deps = [
        ":atomic_indirector",
        ":atomic_indirector_mock_binding",
    ],
)

cc_gtest_unit_test(
    name = "lock_file_test",
    srcs = [
        "lock_file_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "@score-baselibs//score/memory:__pkg__",
    ],
    deps = [
        ":lock_file",
        ":shared_memory_test_resources",
        "@score-baselibs//score/mw/log",
    ],
)

cc_gtest_unit_test(
    name = "managed_memory_resource_test",
    srcs = ["managed_memory_resource_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "@score-baselibs//score/memory:__pkg__",
    ],
    deps = [
        ":fake_memory_resources",
        ":managed_memory_resource",
        ":shared_memory_test_resources",
        "@score-baselibs//score/mw/log",
    ],
)

cc_gtest_unit_test(
    name = "map_test",
    srcs = [
        "map_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "@score-baselibs//score/memory:__pkg__",
    ],
    deps = [
        ":fake_memory_resources",
        ":map",
        "@score-baselibs//score/mw/log",
    ],
)

cc_gtest_unit_test(
    name = "memory_region_map_test",
    srcs = [
        "memory_region_map_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "@score-baselibs//score/memory:__pkg__",
    ],
    deps = [
        ":atomic_indirector_mock_binding",
        ":memory_region_map",
        ":shared_memory_test_resources",
        "@score-baselibs//score/mw/log",
    ],
)

cc_gtest_unit_test(
    name = "memory_resource_proxy_test",
    srcs = [
        "memory_resource_proxy_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "@score-baselibs//score/memory:__pkg__",
    ],
    deps = [
        ":fake_memory_resources",
        ":memory_resource_proxy",
        "@score-baselibs//score/mw/log",
    ],
)

cc_gtest_unit_test(
    name = "memory_resource_registry_test",
    srcs = [
        "memory_resource_registry_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "@score-baselibs//score/memory:__pkg__",
    ],
    deps = [
        ":memory_resource_registry",
        ":pointer_arithmetic_util",
        ":shared_memory_error",
        "@score-baselibs//score/mw/log",
    ],
)

cc_gtest_unit_test(
    name = "new_delete_delegate_resource_test",
    srcs = [
        "new_delete_delegate_resource_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "@score-baselibs//score/memory:__pkg__",
    ],
    deps = [
        ":new_delete_delegate_resource",
        ":shared_memory_test_resources",
    ],
)

cc_gtest_unit_test(
    name = "polymorphic_offset_ptr_allocator_test",
    srcs = [
        "polymorphic_offset_ptr_allocator_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "@score-baselibs//score/memory:__pkg__",
    ],
    deps = [
        ":fake_memory_resources",
        ":polymorphic_offset_ptr_allocator",
        "@score-baselibs//score/mw/log",
    ],
)

cc_gtest_unit_test(
    name = "shared_memory_error_test",
    srcs = [
        "shared_memory_error_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "@score-baselibs//score/memory:__pkg__",
    ],
    deps = [
        ":shared_memory_error",
    ],
)

cc_gtest_unit_test(
    name = "shared_memory_resource_allocate_test",
    srcs = [
        "shared_memory_resource_allocate_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "@score-baselibs//score/memory:__pkg__",
    ],
    deps = [
        ":fake_memory_resources",
        ":pointer_arithmetic_util",
        ":shared_memory_test_resources",
        "@score-baselibs//score/mw/log",
    ],
)

cc_gtest_unit_test(
    name = "shared_memory_resource_create_anonymous_test",
    srcs = [
        "shared_memory_resource_create_anonymous_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "@score-baselibs//score/memory:__pkg__",
    ],
    deps = [
        ":shared_memory_test_resources",
        "@score-baselibs//score/memory/shared/sealedshm/sealedshm_wrapper:sealedshm",
        "@score-baselibs//score/os/utils/acl:mock",
        "@score-baselibs//score/mw/log",
    ],
)

cc_gtest_unit_test(
    name = "shared_memory_resource_create_or_open_test",
    srcs = [
        "shared_memory_resource_create_or_open_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "@score-baselibs//score/memory:__pkg__",
    ],
    deps = [
        ":shared_memory_test_resources",
        "@score-baselibs//score/mw/log",
    ],
)

cc_gtest_unit_test(
    name = "shared_memory_resource_create_test",
    srcs = [
        "shared_memory_resource_create_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "@score-baselibs//score/memory:__pkg__",
    ],
    deps = [
        ":pointer_arithmetic_util",
        ":shared_memory_resource",
        ":shared_memory_test_resources",
        "@score-baselibs//score/os/utils/acl:mock",
        "@score-baselibs//score/mw/log",
    ],
)

cc_gtest_unit_test(
    name = "shared_memory_resource_misc_test",
    srcs = [
        "shared_memory_resource_misc_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "@score-baselibs//score/memory:__pkg__",
    ],
    deps = [
        ":fake_memory_resources",
        ":pointer_arithmetic_util",
        ":shared_memory_test_resources",
        "@score-baselibs//score/mw/log",
    ],
)

cc_gtest_unit_test(
    name = "shared_memory_resource_open_test",
    srcs = [
        "shared_memory_resource_open_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "@score-baselibs//score/memory:__pkg__",
    ],
    deps = [
        ":fake_memory_resources",
        ":shared_memory_test_resources",
        "@score-baselibs//score/os/utils/acl:mock",
        "@score-baselibs//score/mw/log",
    ],
)

cc_gtest_unit_test(
    name = "vector_test",
    srcs = [
        "vector_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "@score-baselibs//score/memory:__pkg__",
    ],
    deps = [
        ":fake_memory_resources",
        ":vector",
        "@score-baselibs//score/mw/log",
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
        "@score-baselibs//score/memory:__pkg__",
    ],
    deps = [
        ":fake_memory_resources",
        ":string",
        "@score-baselibs//score/mw/log",
    ],
)

cc_unit_test_suites_for_host_and_qnx(
    name = "unit_test_suite",
    cc_unit_tests = [
        ":atomic_indirector_test",
        ":lock_file_test",
        ":managed_memory_resource_test",
        ":map_test",
        ":memory_region_bounds_test",
        ":memory_region_map_test",
        ":memory_resource_proxy_test",
        ":memory_resource_registry_test",
        ":new_delete_delegate_resource_test",
        ":polymorphic_offset_ptr_allocator_test",
        ":pointer_arithmetic_util_precondition_violation_test",
        ":shared_memory_error_test",
        ":shared_memory_factory_test",
        ":shared_memory_resource_allocate_test",
        ":shared_memory_resource_create_anonymous_test",
        ":shared_memory_resource_create_or_open_test",
        ":shared_memory_resource_create_test",
        ":shared_memory_resource_misc_test",
        ":shared_memory_resource_open_test",
        ":string_test",
        ":vector_test",
    ],
    test_suites_from_sub_packages = [
        "@score-baselibs//score/memory/shared/flock:unit_test_suite",
        "@score-baselibs//score/memory/shared/sealedshm/sealedshm_wrapper:unit_test_suite",
        "@score-baselibs//score/memory/shared/typedshm/typedshm_wrapper:unit_test_suite",
        "@score-baselibs//score/memory/shared/test_offset_ptr:unit_test_suite",
    ],
    visibility = [
        "@score-baselibs//score/memory:__pkg__",
    ],
)
