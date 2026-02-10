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
    name = "tracing_runtime",
    testonly = True,
    srcs = ["tracing_runtime.cpp"],
    hdrs = ["tracing_runtime.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com/impl:__pkg__",
        "//score/mw/com/impl/bindings/lola:__subpackages__",
        "//score/mw/com/impl/plumbing:__pkg__",
        "//score/mw/com/impl/test:__pkg__",
        "//score/mw/com/impl/tracing:__pkg__",
    ],
    deps = [
        "//score/mw/com/impl/tracing:i_binding_tracing_runtime",
        "@googletest//:gtest",
    ],
)
