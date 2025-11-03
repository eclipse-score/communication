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
load("@score_baselibs//score/language/safecpp:toolchain_features.bzl", "COMPILER_WARNING_FEATURES")

cc_library(
    name = "fake_memory_resources",
    testonly = True,
    srcs = [
        "my_bounded_memory_resource.cpp",
        "my_memory_resource.cpp",
    ],
    hdrs = [
        "my_bounded_memory_resource.h",
        "my_memory_resource.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//visibility:public"],
    deps = [
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/memory/shared:shared_memory_resource",
        "@score_baselibs//score/memory/shared:types",
    ],
)

cc_library(
    name = "my_bounded_shared_memory_resource",
    testonly = True,
    srcs = [
        "my_bounded_shared_memory_resource.cpp",
    ],
    hdrs = [
        "my_bounded_shared_memory_resource.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//visibility:public"],
    deps = [
        ":fake_memory_resources",
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/memory/shared:i_shared_memory_resource",
        "@score_baselibs//score/memory/shared:types",
    ],
)
