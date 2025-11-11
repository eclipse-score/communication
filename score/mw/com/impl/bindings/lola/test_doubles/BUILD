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

load("//score/mw:common_features.bzl", "COMPILER_WARNING_FEATURES")

cc_library(
    name = "fake_memory_resource",
    testonly = True,
    srcs = ["fake_memory_resource.cpp"],
    hdrs = ["fake_memory_resource.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com/impl/bindings/lola:__pkg__",
        "//score/mw/com/impl/plumbing:__pkg__",
    ],
    deps = ["@score_baselibs//score/memory/shared:types"],
)

cc_library(
    name = "test_doubles",
    testonly = True,
    srcs = [
        "fake_mocked_service_data.cpp",
        "fake_service_data.cpp",
    ],
    hdrs = [
        "fake_mocked_service_data.h",
        "fake_service_data.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//score/mw/com/impl:__subpackages__"],
    deps = [
        ":fake_memory_resource",
        "//score/mw/com/impl/bindings/lola:event",
        "//score/mw/com/impl/bindings/lola:shared_data_structures",
        "//score/mw/com/impl/bindings/lola:skeleton",
        "@score_baselibs//score/memory/shared",
        "@score_baselibs//score/memory/shared:shared_memory_resource_heap_allocator_mock",
    ],
)
