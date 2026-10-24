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

load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")
load("//score/mw:common_features.bzl", "COMPILER_WARNING_FEATURES")

cc_library(
    name = "someip_serialization_client",
    srcs = ["someip_serialization_client.cpp"],
    hdrs = ["someip_serialization_client.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = [
        "//score/mw/com/impl:__subpackages__",
    ],
    deps = [
        "//score/mw/com/impl:instance_specifier",
        "//score/mw/com/impl/serialization:serialization_request",
        "//score/mw/com/impl/serialization:serialization_service",
        "//score/mw/com/impl/serialization:serializer",
        "@score_baselibs//score/result",
    ],
)

cc_test(
    name = "someip_serialization_client_test",
    size = "small",
    timeout = "short",
    srcs = ["someip_serialization_client_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":someip_serialization_client",
        "//score/mw/com/impl:error",
        "//score/mw/com/impl:instance_specifier",
        "//score/mw/com/impl/serialization:serialization_infrastructure",
        "//score/mw/com/impl/serialization:serialization_orchestrator",
        "//score/mw/com/impl/serialization:serialization_request",
        "//score/mw/com/impl/serialization:serialization_service",
        "//score/mw/com/impl/serialization:serializer",
        "//score/mw/com/impl/serialization:serializer_key_resolver",
        "//score/mw/com/impl/serialization:serializer_lookup_key",
        "@score_baselibs//score/os/mocklib:dlfcn_mock",
        "@googletest//:gtest_main",
    ],
)
