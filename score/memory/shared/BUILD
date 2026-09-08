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
load("//quality/unit_testing:unit_testing.bzl", "cc_unit_test")

# TODO move to //score
load("//score/mw:common_features.bzl", "COMPILER_WARNING_FEATURES")

cc_library(
    name = "pointer_arithmetic_util",
    srcs = ["pointer_arithmetic_util.cpp"],
    hdrs = ["pointer_arithmetic_util.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//visibility:public",  # platform_only
    ],
    deps = [
        "@score_baselibs//score/language/safecpp/safe_math",
        "@score_baselibs//score/memory:data_type_size_info",
        "@score_baselibs//score/mw/log:frontend",
    ],
)

cc_library(
    name = "i_shared_memory_resource",
    srcs = ["i_shared_memory_resource.cpp"],
    hdrs = ["i_shared_memory_resource.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//visibility:public",  # platform_only
    ],
    deps = [
        ":memory_resource_registry",
        ":types",
        ":user_permission",
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/os/utils/acl",
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
        "//visibility:public",  # platform_only
    ],
    deps = [
        ":i_shared_memory_resource",
        ":new_delete_delegate_resource",
        "@googletest//:gtest",
        "@score_baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "shared_memory_resource_mock",
    testonly = True,
    srcs = ["shared_memory_resource_mock.cpp"],
    hdrs = ["shared_memory_resource_mock.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//visibility:public",  # platform_only
    ],
    deps = [
        ":i_shared_memory_resource",
        "@googletest//:gtest",
        "@score_baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "shared_memory_resource",
    srcs = ["shared_memory_resource.cpp"],
    hdrs = ["shared_memory_resource.h"],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = [
        "@score_baselibs//score/language/safecpp/string_view:zstring_view",
    ],
    tags = ["FFI"],
    visibility = [
        "//score/memory/shared/fake:__subpackages__",
    ],
    deps = [
        ":i_shared_memory_resource",
        ":lock_file",
        ":pointer_arithmetic_util",
        ":types",
        "//score/memory/shared/sealedshm/sealedshm_wrapper:sealedshm",
        "//score/memory/shared/typedshm/typedshm_wrapper:typedmemory",
        "//score/memory/shared/typedshm/utils:typed_memory_utils",
        "@score_baselibs//score/concurrency:atomic_indirector",
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/language/safecpp/safe_math",
        "@score_baselibs//score/mw/log:frontend",
        "@score_baselibs//score/os:errno_logging",
        "@score_baselibs//score/os:fcntl",
        "@score_baselibs//score/os:mman",
        "@score_baselibs//score/os:stat",
        "@score_baselibs//score/os:unistd",
        "@score_baselibs//score/os/utils/acl",
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
        "//score/memory/shared/typedshm/typedshm_wrapper:typedmemory",
        "@score_baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "shared_memory_factory_mock",
    testonly = True,
    srcs = ["shared_memory_factory_mock.cpp"],
    hdrs = ["shared_memory_factory_mock.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//visibility:public",  # platform_only
    ],
    deps = [
        ":i_shared_memory_factory",
        ":types",
        "//score/memory/shared/typedshm/typedshm_wrapper:typedmemory",
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
        "//score/memory/shared/typedshm/typedshm_wrapper:typedmemory",
        "//score/memory/shared/typedshm/utils:typed_memory_utils",
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/mw/log:frontend",
        "@score_baselibs//score/os:mman",
        "@score_baselibs//score/os:unistd",
        "@score_baselibs//score/os/utils/acl",
        "@score_baselibs//score/utils:static_destruction_guard",
    ],
)

cc_library(
    name = "shared_memory_factory",
    srcs = ["shared_memory_factory.cpp"],
    hdrs = ["shared_memory_factory.h"],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = [
        "shared_memory_factory_impl",
        "@score_baselibs//score/mw/log:frontend",
        "@score_baselibs//score/os:errno_logging",
        "@score_baselibs//score/os:unistd",
        "@score_baselibs//score/utils:static_destruction_guard",
    ],
    tags = ["FFI"],
    deps = [
        ":i_shared_memory_factory",
        ":i_shared_memory_resource",
        ":types",
        "@score_baselibs//score/language/futurecpp",
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
        "@score_baselibs//score/mw/log:frontend",
        "@score_baselibs//score/os:fcntl",
        "@score_baselibs//score/os:stat",
        "@score_baselibs//score/os:unistd",
    ],
    tags = ["FFI"],
    visibility = [
        "//visibility:public",  # platform_only
    ],
    deps = [
        "@score_baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "managed_memory_resource",
    srcs = ["managed_memory_resource.cpp"],
    hdrs = ["managed_memory_resource.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//visibility:public",  # platform_only
    ],
    deps = ["@score_baselibs//score/language/futurecpp"],
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
        "//visibility:public",  # platform_only
    ],
    deps = [
        ":memory_region_bounds",
        "@score_baselibs//score/concurrency:atomic_indirector",
        "@score_baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "memory_region_bounds",
    srcs = ["memory_region_bounds.cpp"],
    hdrs = ["memory_region_bounds.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    deps = [
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
        "//visibility:public",  # platform_only
    ],
    deps = [
        ":managed_memory_resource",
        ":memory_region_bounds",
        ":memory_region_map",
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/result",
        "@score_baselibs//score/utils:static_destruction_guard",
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
        "//visibility:public",  # platform_only
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
        "//visibility:public",  # platform_only
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
        "//visibility:public",  # platform_only
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
        "//visibility:public",  # platform_only
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
        "//visibility:public",  # platform_only
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
        "//visibility:public",  # platform_only
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
        "//visibility:public",  # platform_only
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
    visibility = ["//score/memory/shared:__subpackages__"],
    deps = [
        ":shared",
        "//score/memory/shared/sealedshm/sealedshm_wrapper:sealedshm_mock",
        "//score/memory/shared/typedshm/typedshm_wrapper/test:typedmemory_mock",
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/os/mocklib:acl_mock",
        "@score_baselibs//score/os/mocklib:fcntl_mock",
        "@score_baselibs//score/os/mocklib:mman_mock",
        "@score_baselibs//score/os/mocklib:stat_mock",
        "@score_baselibs//score/os/mocklib:unistd_mock",
    ],
)

cc_library(
    name = "user_permission",
    srcs = ["user_permission.cpp"],
    hdrs = ["user_permission.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = ["//score/memory/shared:__subpackages__"],
    deps = ["@score_baselibs//score/os/utils/acl"],
)

cc_library(
    name = "new_delete_delegate_resource",
    srcs = ["new_delete_delegate_resource.cpp"],
    hdrs = ["new_delete_delegate_resource.h"],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = [
        "@score_baselibs//score/language/safecpp/safe_math",
        "@score_baselibs//score/mw/log:frontend",
    ],
    tags = ["FFI"],
    visibility = [
        "//visibility:public",
    ],
    deps = [
        ":pointer_arithmetic_util",
        ":shared",
        ":types",
        "@score_baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "shared_memory_error",
    srcs = ["shared_memory_error.cpp"],
    hdrs = ["shared_memory_error.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = ["//score/memory/shared:__pkg__"],
    deps = [
        "@score_baselibs//score/result",
    ],
)

cc_unit_test(
    name = "pointer_arithmetic_util_precondition_violation_test",
    srcs = ["pointer_arithmetic_util_test.cpp"],
    features = COMPILER_WARNING_FEATURES + [
        # These tests catch exceptions instead of using gtest EXPECT_DEATH so we disable aborts_upon_exception.
        "-aborts_upon_exception",
    ],
    visibility = [
        "//score/memory:__pkg__",
    ],
    deps = [
        ":pointer_arithmetic_util",
        "@score_baselibs//score/language/futurecpp:futurecpp_test_support",
        "@score_baselibs//score/mw/log:backend_stub_testutil",
    ],
)

cc_unit_test(
    name = "pointer_arithmetic_util_calculate_aligned_size_test",
    srcs = [
        "pointer_arithmetic_util_calculate_aligned_size_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES + [
        # These tests catch exceptions instead of using gtest EXPECT_DEATH so we disable aborts_upon_exception.
        "-aborts_upon_exception",
    ],
    visibility = [
        "//score/memory:__pkg__",
    ],
    deps = [
        ":pointer_arithmetic_util",
        "@score_baselibs//score/language/futurecpp:futurecpp_test_support",
        "@score_baselibs//score/mw/log:backend_stub_testutil",
    ],
)

cc_unit_test(
    name = "shared_memory_factory_test",
    srcs = [
        "shared_memory_factory_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES,
    target_compatible_with = ["@platforms//os:linux"],
    visibility = [
        "//score/memory:__pkg__",
    ],
    deps = [
        ":shared_memory_factory",
        ":shared_memory_test_resources",
        "//score/memory/shared/sealedshm/sealedshm_wrapper:sealedshm",
        "@score_baselibs//score/mw/log:backend_stub_testutil",
    ],
)

cc_unit_test(
    name = "memory_region_bounds_test",
    srcs = [
        "memory_region_bounds_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/memory:__pkg__",
    ],
    deps = [
        ":memory_region_bounds",
    ],
)

cc_unit_test(
    name = "lock_file_test",
    srcs = [
        "lock_file_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/memory:__pkg__",
    ],
    deps = [
        ":lock_file",
        ":shared_memory_test_resources",
        "@score_baselibs//score/mw/log:backend_stub_testutil",
    ],
)

cc_unit_test(
    name = "managed_memory_resource_test",
    srcs = ["managed_memory_resource_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/memory:__pkg__",
    ],
    deps = [
        ":managed_memory_resource",
        ":shared_memory_test_resources",
        "//score/memory/shared/fake:fake_memory_resources",
        "@score_baselibs//score/mw/log:backend_stub_testutil",
    ],
)

cc_unit_test(
    name = "map_test",
    srcs = [
        "map_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/memory:__pkg__",
    ],
    deps = [
        ":map",
        "//score/memory/shared/fake:fake_memory_resources",
        "@score_baselibs//score/mw/log:backend_stub_testutil",
    ],
)

cc_unit_test(
    name = "memory_region_map_test",
    srcs = [
        "memory_region_map_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/memory:__pkg__",
    ],
    deps = [
        ":memory_region_map",
        ":shared_memory_test_resources",
        "@score_baselibs//score/concurrency:atomic_indirector_mock_binding",
        "@score_baselibs//score/mw/log:backend_stub_testutil",
    ],
)

cc_unit_test(
    name = "memory_resource_proxy_test",
    srcs = [
        "memory_resource_proxy_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/memory:__pkg__",
    ],
    deps = [
        ":memory_resource_proxy",
        ":shared_memory_test_resources",
        "//score/memory/shared/fake:fake_memory_resources",
        "@score_baselibs//score/mw/log:backend_stub_testutil",
    ],
)

cc_unit_test(
    name = "memory_resource_registry_test",
    srcs = [
        "memory_resource_registry_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/memory:__pkg__",
    ],
    deps = [
        ":memory_resource_registry",
        ":pointer_arithmetic_util",
        ":shared_memory_error",
        "@score_baselibs//score/mw/log:backend_stub_testutil",
    ],
)

cc_unit_test(
    name = "new_delete_delegate_resource_test",
    srcs = [
        "new_delete_delegate_resource_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/memory:__pkg__",
    ],
    deps = [
        ":new_delete_delegate_resource",
        ":shared_memory_test_resources",
        "@score_baselibs//score/mw/log:backend_stub_testutil",
    ],
)

cc_unit_test(
    name = "polymorphic_offset_ptr_allocator_test",
    srcs = [
        "polymorphic_offset_ptr_allocator_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/memory:__pkg__",
    ],
    deps = [
        ":polymorphic_offset_ptr_allocator",
        "//score/memory/shared/fake:fake_memory_resources",
        "@score_baselibs//score/mw/log:backend_stub_testutil",
    ],
)

cc_unit_test(
    name = "containers_compatibility_test",
    srcs = [
        "containers_compatibility_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/memory:__pkg__",
    ],
    deps = [
        ":polymorphic_offset_ptr_allocator",
        "//score/memory/shared/fake:fake_memory_resources",
        "@score_baselibs//score/containers:dynamic_array",
        "@score_baselibs//score/containers:non_relocatable_vector",
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/mw/log:backend_stub_testutil",
    ],
)

cc_unit_test(
    name = "shared_memory_error_test",
    srcs = [
        "shared_memory_error_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/memory:__pkg__",
    ],
    deps = [
        ":shared_memory_error",
    ],
)

cc_unit_test(
    name = "shared_memory_resource_allocate_test",
    srcs = [
        "shared_memory_resource_allocate_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/memory:__pkg__",
    ],
    deps = [
        ":pointer_arithmetic_util",
        ":shared_memory_test_resources",
        "//score/memory/shared/fake:fake_memory_resources",
        "@score_baselibs//score/concurrency:atomic_indirector_mock_binding",
        "@score_baselibs//score/mw/log:backend_stub_testutil",
    ],
)

cc_unit_test(
    name = "shared_memory_resource_create_anonymous_test",
    srcs = [
        "shared_memory_resource_create_anonymous_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/memory:__pkg__",
    ],
    deps = [
        ":shared_memory_test_resources",
        "//score/memory/shared/sealedshm/sealedshm_wrapper:sealedshm",
        "@score_baselibs//score/mw/log:backend_stub_testutil",
        "@score_baselibs//score/os/utils/acl:mock",
    ],
)

cc_unit_test(
    name = "shared_memory_resource_create_or_open_test",
    srcs = [
        "shared_memory_resource_create_or_open_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/memory:__pkg__",
    ],
    deps = [
        ":shared_memory_test_resources",
        "@score_baselibs//score/mw/log:backend_stub_testutil",
    ],
)

cc_unit_test(
    name = "shared_memory_resource_create_test",
    srcs = [
        "shared_memory_resource_create_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/memory:__pkg__",
    ],
    deps = [
        ":pointer_arithmetic_util",
        ":shared_memory_resource",
        ":shared_memory_test_resources",
        "@score_baselibs//score/mw/log:backend_stub_testutil",
        "@score_baselibs//score/os/utils/acl:mock",
    ],
)

cc_unit_test(
    name = "shared_memory_resource_misc_test",
    srcs = [
        "shared_memory_resource_misc_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/memory:__pkg__",
    ],
    deps = [
        ":pointer_arithmetic_util",
        ":shared_memory_test_resources",
        "//score/memory/shared/fake:fake_memory_resources",
        "//score/memory/shared/sealedshm/sealedshm_wrapper:sealedshm_mock",
        "@score_baselibs//score/mw/log:backend_stub_testutil",
    ],
)

cc_unit_test(
    name = "shared_memory_resource_open_test",
    srcs = [
        "shared_memory_resource_open_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES,
    target_compatible_with = ["@platforms//os:linux"],
    visibility = [
        "//score/memory:__pkg__",
    ],
    deps = [
        ":shared_memory_test_resources",
        "//score/memory/shared/fake:fake_memory_resources",
        "@score_baselibs//score/mw/log:backend_stub_testutil",
        "@score_baselibs//score/os/utils/acl:mock",
    ],
)

cc_unit_test(
    name = "vector_test",
    srcs = [
        "vector_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/memory:__pkg__",
    ],
    deps = [
        ":vector",
        "//score/memory/shared/fake:fake_memory_resources",
        "@score_baselibs//score/mw/log:backend_stub_testutil",
    ],
)

cc_unit_test(
    name = "string_test",
    srcs = [
        "string_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES,
    target_compatible_with = [
        "@platforms//os:linux",
    ],
    visibility = [
        "//score/memory:__pkg__",
    ],
    deps = [
        ":string",
        "//score/memory/shared/fake:fake_memory_resources",
        "@score_baselibs//score/mw/log:backend_stub_testutil",
    ],
)
