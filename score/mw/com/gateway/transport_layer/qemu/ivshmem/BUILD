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
load("@rules_cc//cc:defs.bzl", "cc_library")
load("@score_baselibs//score/language/safecpp:toolchain_features.bzl", "COMPILER_WARNING_FEATURES")
load("//quality/unit_testing:unit_testing.bzl", "cc_unit_test")

cc_library(
    name = "ivshmem_bar_discovery",
    srcs = ["ivshmem_bar_discovery.cpp"],
    hdrs = ["ivshmem_bar_discovery.h"],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    linkopts = select({
        "@platforms//os:qnx": ["-lpci"],
        "//conditions:default": [],
    }),
    visibility = ["//score/mw/com/gateway/transport_layer:__subpackages__"],
    deps = [
        "@score_baselibs//score/mw/log",
    ],
)

cc_library(
    name = "ivshmem_typed_memory_provider",
    srcs = ["ivshmem_typed_memory_provider.cpp"],
    hdrs = ["ivshmem_typed_memory_provider.h"],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    visibility = ["//score/mw/com/gateway/transport_layer:__subpackages__"],
    deps = [
        "//score/memory/shared",
        "//score/memory/shared/typedshm/typedshm_wrapper:typedmemory",
        "@score_baselibs//score/mw/log",
        "@score_baselibs//score/os:errno",
        "@score_baselibs//score/os:mman",
    ] + select({
        "@platforms//os:qnx": ["@score_baselibs//score/os/qnx:mman"],
        "//conditions:default": [],
    }),
)

cc_library(
    name = "ivshmem_typed_memory_provider_mock",
    srcs = ["ivshmem_typed_memory_provider_mock.cpp"],
    hdrs = ["ivshmem_typed_memory_provider_mock.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//score/mw/com/gateway/transport_layer:__subpackages__"],
    deps = [
        ":ivshmem_typed_memory_provider",
        "@googletest//:gtest",
    ],
)

cc_unit_test(
    name = "ivshmem_typed_memory_provider_test",
    srcs = ["ivshmem_typed_memory_provider_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":ivshmem_typed_memory_provider",
        "@googletest//:gtest",
        "@score_baselibs//score/mw/log",
        "@score_baselibs//score/os/mocklib:mman_mock",
    ] + select({
        "@platforms//os:qnx": ["@score_baselibs//score/os/mocklib/qnx:mman_mock"],
        "//conditions:default": [],
    }),
)

cc_unit_test(
    name = "ivshmem_bar_discovery_test",
    srcs = ["ivshmem_bar_discovery_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":ivshmem_bar_discovery",
        "@score_baselibs//score/mw/log",
    ],
)
