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
        "@score_baselibs//score/memory:data_type_size_info",
        "@score_baselibs//score/memory/shared:pointer_arithmetic_util",
    ],
)
