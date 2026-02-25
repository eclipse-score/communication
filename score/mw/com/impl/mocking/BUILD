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
load("@score_baselibs//:bazel/unit_tests.bzl", "cc_gtest_unit_test", "cc_unit_test_suites_for_host_and_qnx")
load("//score/mw:common_features.bzl", "COMPILER_WARNING_FEATURES")

cc_library(
    name = "test_type_utilities",
    testonly = True,
    srcs = ["test_type_utilities.cpp"],
    hdrs = ["test_type_utilities.h"],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = [
        "//score/mw/com/impl:instance_specifier",
        "//score/mw/com/impl/configuration",
    ],
    visibility = [
        "//score/mw/com:__subpackages__",
    ],
    deps = [
        "//score/mw/com/impl:handle_type",
        "//score/mw/com/impl:instance_identifier",
        "//score/mw/com/impl/plumbing:sample_allocatee_ptr",
        "//score/mw/com/impl/plumbing:sample_ptr",
    ],
)

cc_library(
    name = "i_proxy_event",
    srcs = ["i_proxy_event.cpp"],
    hdrs = ["i_proxy_event.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com/impl:__subpackages__",
    ],
    deps = [
        "//score/mw/com/impl:event_receive_handler",
        "//score/mw/com/impl:subscription_state",
        "//score/mw/com/impl/plumbing:sample_ptr",
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/result",
    ],
)

cc_library(
    name = "proxy_event_mock",
    testonly = True,
    srcs = ["proxy_event_mock.cpp"],
    hdrs = ["proxy_event_mock.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com/impl:__subpackages__",
    ],
    deps = [
        ":i_proxy_event",
        "@googletest//:gtest",
    ],
)

cc_library(
    name = "i_skeleton_base",
    srcs = ["i_skeleton_base.cpp"],
    hdrs = ["i_skeleton_base.h"],
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

cc_library(
    name = "skeleton_base_mock",
    testonly = True,
    srcs = ["skeleton_base_mock.cpp"],
    hdrs = ["skeleton_base_mock.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com/impl:__subpackages__",
    ],
    deps = [
        ":i_skeleton_base",
        "@googletest//:gtest",
    ],
)

cc_library(
    name = "i_skeleton_event",
    srcs = ["i_skeleton_event.cpp"],
    hdrs = ["i_skeleton_event.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com/impl:__subpackages__",
    ],
    deps = [
        "//score/mw/com/impl/plumbing:sample_allocatee_ptr",
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/result",
    ],
)

cc_library(
    name = "skeleton_event_mock",
    testonly = True,
    srcs = ["skeleton_event_mock.cpp"],
    hdrs = ["skeleton_event_mock.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com/impl:__subpackages__",
    ],
    deps = [
        ":i_skeleton_event",
        "@googletest//:gtest",
    ],
)

cc_library(
    name = "i_skeleton_field",
    srcs = ["i_skeleton_field.cpp"],
    hdrs = ["i_skeleton_field.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com/impl:__subpackages__",
    ],
    deps = [
        "//score/mw/com/impl/plumbing:sample_allocatee_ptr",
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/result",
    ],
)

cc_library(
    name = "skeleton_field_mock",
    testonly = True,
    srcs = ["skeleton_field_mock.cpp"],
    hdrs = ["skeleton_field_mock.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com/impl:__subpackages__",
    ],
    deps = [
        "i_skeleton_field",
        "@googletest//:gtest",
    ],
)

cc_library(
    name = "skeleton_wrapper_class_test_view",
    testonly = True,
    srcs = ["skeleton_wrapper_class_test_view.cpp"],
    hdrs = ["skeleton_wrapper_class_test_view.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com:__subpackages__",
    ],
    deps = [
        ":skeleton_base_mock",
        ":skeleton_event_mock",
        ":skeleton_field_mock",
        ":test_type_utilities",
        "//score/mw/com/impl:instance_identifier",
        "//score/mw/com/impl:instance_specifier",
        "//score/mw/com/impl:traits",
        "//score/mw/com/impl/test:binding_factory_resources",
        "@score_baselibs//score/result",
    ],
)

cc_library(
    name = "proxy_wrapper_class_test_view",
    testonly = True,
    srcs = ["proxy_wrapper_class_test_view.cpp"],
    hdrs = ["proxy_wrapper_class_test_view.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com:__subpackages__",
    ],
    deps = [
        ":proxy_event_mock",
        ":test_type_utilities",
        "//score/mw/com/impl:traits",
        "//score/mw/com/impl/test:binding_factory_resources",
        "@score_baselibs//score/result",
    ],
)

cc_gtest_unit_test(
    name = "test_type_utilities_test",
    srcs = ["test_type_utilities_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":test_type_utilities",
    ],
)

cc_gtest_unit_test(
    name = "skeleton_base_mock_test",
    srcs = ["skeleton_base_mock_test.cpp"],
    deps = [
        ":skeleton_base_mock",
        ":test_type_utilities",
        "//score/mw/com/impl:skeleton_base",
    ],
)

cc_gtest_unit_test(
    name = "skeleton_event_mock_test",
    srcs = ["skeleton_event_mock_test.cpp"],
    deps = [
        ":skeleton_event_mock",
        ":test_type_utilities",
        "//score/mw/com/impl:skeleton_event",
    ],
)

cc_gtest_unit_test(
    name = "skeleton_field_mock_test",
    srcs = ["skeleton_field_mock_test.cpp"],
    deps = [
        ":skeleton_field_mock",
        ":test_type_utilities",
        "//score/mw/com/impl:skeleton_field",
    ],
)

cc_gtest_unit_test(
    name = "proxy_event_mock_test",
    srcs = ["proxy_event_mock_test.cpp"],
    deps = [
        ":proxy_event_mock",
        ":test_type_utilities",
        "//score/mw/com/impl:proxy_event",
        "//score/mw/com/impl:proxy_field",
    ],
)

cc_gtest_unit_test(
    name = "skeleton_wrapper_class_test_view_test",
    srcs = ["skeleton_wrapper_class_test_view_test.cpp"],
    deps = [
        ":skeleton_base_mock",
        ":skeleton_event_mock",
        ":skeleton_field_mock",
        ":skeleton_wrapper_class_test_view",
        ":test_type_utilities",
        "//score/mw/com:test_types",
    ],
)

cc_gtest_unit_test(
    name = "proxy_wrapper_class_test_view_test",
    srcs = ["proxy_wrapper_class_test_view_test.cpp"],
    deps = [
        ":proxy_event_mock",
        ":proxy_wrapper_class_test_view",
        "//score/mw/com:test_types",
    ],
)

cc_unit_test_suites_for_host_and_qnx(
    name = "unit_test_suite",
    cc_unit_tests = [
        ":skeleton_event_mock_test",
        ":skeleton_field_mock_test",
        ":skeleton_base_mock_test",
        ":test_type_utilities_test",
        ":proxy_event_mock_test",
        ":proxy_wrapper_class_test_view_test",
        ":skeleton_wrapper_class_test_view_test",
    ],
    test_suites_from_sub_packages = [],
    visibility = ["//score/mw/com/impl:__pkg__"],
)
