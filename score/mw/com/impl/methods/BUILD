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
    name = "method_signature_element_ptr",
    srcs = ["method_signature_element_ptr.cpp"],
    hdrs = ["method_signature_element_ptr.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com:__subpackages__",
    ],
    deps = [
    ],
)

cc_library(
    name = "proxy_method_base",
    srcs = ["proxy_method_base.cpp"],
    hdrs = ["proxy_method_base.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com:__subpackages__",
    ],
    deps = [
        ":proxy_method_binding",
        "@score_baselibs//score/containers:dynamic_array",
        "@score_baselibs//score/memory:data_type_size_info",
        "@score_baselibs//score/result",
    ],
)

cc_library(
    name = "proxy_method",
    srcs = [
        "proxy_method.cpp",
        "proxy_method_with_in_args.cpp",
        "proxy_method_with_in_args_and_return.cpp",
        "proxy_method_with_return_type.cpp",
        "proxy_method_without_in_args_or_return.cpp",
    ],
    hdrs = [
        "proxy_method.h",
        "proxy_method_with_in_args.h",
        "proxy_method_with_in_args_and_return.h",
        "proxy_method_with_return_type.h",
        "proxy_method_without_in_args_or_return.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com:__subpackages__",
    ],
    deps = [
        ":method_signature_element_ptr",
        ":proxy_method_base",
        ":proxy_method_binding",
        "//score/mw/com/impl:error",
        "//score/mw/com/impl:proxy_base",
        "//score/mw/com/impl/plumbing:proxy_method_binding_factory",
        "//score/mw/com/impl/util:type_erased_storage",
        "@score_baselibs//score/containers:dynamic_array",
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/result",
    ],
)

cc_library(
    name = "proxy_method_binding",
    srcs = [
        "proxy_method_binding.cpp",
    ],
    hdrs = [
        "proxy_method_binding.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com/impl/bindings/lola:__pkg__",
        "//score/mw/com/impl/bindings/mock_binding:__pkg__",
        "//score/mw/com/impl/plumbing:__subpackages__",
    ],
    deps = [
        "//score/mw/com/impl/util:type_erased_storage",
        "@score_baselibs//score/memory:data_type_size_info",
        "@score_baselibs//score/result",
    ],
)

cc_gtest_unit_test(
    name = "method_signature_element_ptr_test",
    srcs = ["method_signature_element_ptr_test.cpp"],
    deps = [
        ":method_signature_element_ptr",
    ],
)

cc_unit_test(
    name = "proxy_method_test",
    srcs = ["proxy_method_test.cpp"],
    features = COMPILER_WARNING_FEATURES + [
        # These tests catch exceptions instead of using gtest EXPECT_DEATH so we disable aborts_upon_exception.
        "-aborts_upon_exception",
    ],
    deps = [
        ":proxy_method",
        "//score/mw/com/impl/bindings/mock_binding",
        "//score/mw/com/impl/configuration/test:configuration_store",
        "//score/mw/com/impl/plumbing:proxy_method_binding_factory_mock",
        "@googletest//:gtest",
        "@googletest//:gtest_main",
        "@score_baselibs//score/language/futurecpp:futurecpp_test_support",
    ],
)

cc_library(
    name = "skeleton_method_base",
    srcs = ["skeleton_method_base.cpp"],
    hdrs = ["skeleton_method_base.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com:__subpackages__",
    ],
    deps = [
        ":skeleton_method_binding",
        "//score/mw/com/impl/util:type_erased_storage",
    ],
)

cc_library(
    name = "skeleton_method",
    srcs = ["skeleton_method.cpp"],
    hdrs = ["skeleton_method.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com/impl:__subpackages__",
    ],
    deps = [
        ":skeleton_method_binding",
        "//score/mw/com/impl:instance_identifier",
        "//score/mw/com/impl:skeleton_base",
        "//score/mw/com/impl/plumbing:skeleton_method_binding_factory",
        "@score_baselibs//score/result",
    ],
)

cc_library(
    name = "skeleton_method_binding",
    srcs = ["skeleton_method_binding.cpp"],
    hdrs = ["skeleton_method_binding.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com/impl:__subpackages__",
    ],
    deps = [
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/result",
    ],
)

cc_gtest_unit_test(
    name = "skeleton_method_test",
    srcs = [
        "skeleton_method_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":skeleton_method",
        "//score/mw/com/impl/bindings/mock_binding",
        "//score/mw/com/impl/plumbing:skeleton_method_binding_factory_mock",
        "@score_baselibs//score/language/futurecpp",
    ],
)

cc_gtest_unit_test(
    name = "skeleton_method_base_test",
    srcs = [
        "skeleton_method_base_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":skeleton_method",
        ":skeleton_method_base",
        "//score/mw/com/impl/bindings/mock_binding",
        "@score_baselibs//score/language/futurecpp",
    ],
)

cc_unit_test_suites_for_host_and_qnx(
    name = "unit_test_suite",
    cc_unit_tests = [
        ":method_signature_element_ptr_test",
        ":proxy_method_test",
        "skeleton_method_base_test",
        "skeleton_method_test",
    ],
    visibility = ["//score/mw/com/impl:__pkg__"],
)
