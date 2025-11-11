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

load("@score_baselibs//:bazel/unit_tests.bzl", "cc_gtest_unit_test")
load("//score/mw:common_features.bzl", "COMPILER_WARNING_FEATURES")

cc_library(
    name = "copyable_atomic",
    srcs = ["copyable_atomic.cpp"],
    hdrs = ["copyable_atomic.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = ["//score/mw/com/impl:__subpackages__"],
    deps = [
    ],
)

cc_library(
    name = "arithmetic_utils",
    srcs = ["arithmetic_utils.cpp"],
    hdrs = ["arithmetic_utils.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = ["//score/mw/com/impl:__subpackages__"],
    deps = [
    ],
)

cc_library(
    name = "type_erased_storage",
    srcs = ["type_erased_storage.cpp"],
    hdrs = ["type_erased_storage.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = ["//score/mw/com/impl:__subpackages__"],
    deps = [
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/memory/shared:pointer_arithmetic_util",
    ],
)

cc_gtest_unit_test(
    name = "copyable_atomic_test",
    srcs = ["copyable_atomic_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":copyable_atomic",
    ],
)

cc_gtest_unit_test(
    name = "arithmetic_utils_test",
    srcs = ["arithmetic_utils_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":arithmetic_utils",
        "@score_baselibs//score/language/futurecpp",
    ],
)

cc_gtest_unit_test(
    name = "type_erased_storage_test",
    srcs = ["type_erased_storage_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":type_erased_storage",
        "@score_baselibs//score/mw/log",
    ],
)
