# *******************************************************************************
# Copyright (c) 2026 Contributors to the Eclipse Foundation
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

load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library", "cc_test")
load("//score/mw:common_features.bzl", "COMPILER_WARNING_FEATURES")

package(
    default_visibility = ["//score/mw/com:__subpackages__"],
)

# Big-endian (SOME/IP network byte order) read/write helpers shared by every generated serializer.
cc_library(
    name = "serialization_helpers",
    srcs = ["serialization_helpers.cpp"],
    hdrs = ["serialization_helpers.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com:__subpackages__",
    ],
    deps = ["@score_baselibs//score/language/futurecpp"],
)

# Backend helper that owns dlopen/dlsym/dlclose for generated serializer libraries.
cc_library(
    name = "serializer_library",
    srcs = ["serializer_library.cpp"],
    hdrs = ["serializer_library.h"],
    features = COMPILER_WARNING_FEATURES,
    linkopts = ["-ldl"],
    tags = ["FFI"],
    visibility = [
        "//score/mw/com:__subpackages__",
    ],
    deps = [],
)

cc_binary(
    name = "backend_logic_test_plugin.so",
    srcs = ["backend_logic_test_plugin.cpp"],
    features = COMPILER_WARNING_FEATURES,
    linkshared = True,
)

cc_test(
    name = "backend_logic_integration_test",
    size = "small",
    timeout = "short",
    srcs = ["backend_logic_integration_test.cpp"],
    data = [":backend_logic_test_plugin.so"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":serializer_library",
        "@bazel_tools//tools/cpp/runfiles",
        "@googletest//:gtest_main",
    ],
)
