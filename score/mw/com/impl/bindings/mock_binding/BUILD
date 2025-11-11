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
    name = "mock_binding",
    testonly = True,
    srcs = [
        "generic_proxy_event.cpp",
        "proxy.cpp",
        "proxy_event.cpp",
        "skeleton.cpp",
        "skeleton_event.cpp",
    ],
    hdrs = [
        "generic_proxy_event.h",
        "proxy.h",
        "proxy_event.h",
        "skeleton.h",
        "skeleton_event.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com/impl:__subpackages__",
        "//score/mw/com/impl/bindings/lola:__subpackages__",
    ],
    deps = [
        ":sample_ptr",
        "//score/mw/com/impl:generic_proxy_event_binding",
        "//score/mw/com/impl:proxy_binding",
        "//score/mw/com/impl:proxy_event_binding",
        "//score/mw/com/impl:skeleton_binding",
        "//score/mw/com/impl:skeleton_event_binding",
        "//score/mw/com/impl/plumbing:sample_allocatee_ptr",
        "@googletest//:gtest",
        "@score_baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "sample_ptr",
    srcs = ["sample_ptr.cpp"],
    hdrs = ["sample_ptr.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = ["//score/mw/com/impl/plumbing:__pkg__"],
    deps = [
        "@score_baselibs//score/language/futurecpp",
    ],
)
