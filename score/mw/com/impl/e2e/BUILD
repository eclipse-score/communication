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
load("//score/mw:common_features.bzl", "COMPILER_WARNING_FEATURES")

cc_library(
    name = "e2e_result",
    hdrs = ["e2e_result.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = ["//score/mw/com:__subpackages__"],
)

cc_library(
    name = "e2e_profile_stub",
    srcs = ["e2e_profile_stub.cpp"],
    hdrs = [
        "e2e_profile_stub.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = ["//score/mw/com:__subpackages__"],
    deps = [
        ":e2e_result",
        "@score_baselibs//score/language/futurecpp",
    ],
)
