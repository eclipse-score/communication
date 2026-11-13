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

load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")
load("//quality/unit_testing:unit_testing.bzl", "cc_unit_test")

cc_library(
    name = "serialization_registry",
    srcs = ["serialization_registry.cpp"],
    hdrs = ["serialization_registry.h"],
    tags = ["FFI"],
    visibility = [
        "//score/mw/com:__subpackages__",
    ],
    deps = [
        "//score/mw/com/impl:error",
        "@score_baselibs//score/os:dlfcn",
        "@score_baselibs//score/result",
    ],
)

cc_library(
    name = "serializer",
    srcs = ["serializer.cpp"],
    hdrs = ["serializer.h"],
    tags = ["FFI"],
    visibility = [
        "//score/mw/com:__subpackages__",
    ],
    deps = [
        ":serialization_registry",
        "//score/mw/com/impl:error",
        "//score/mw/com/impl:instance_specifier",
        "@score_baselibs//score/result",
    ],
)

cc_binary(
    name = "serialization_infrastructure_test_plugin.so",
    srcs = ["serialization_infrastructure_test_plugin.cpp"],
    linkshared = True,
)

cc_unit_test(
    name = "serialization_infrastructure_integration_test",
    srcs = ["serialization_infrastructure_integration_test.cpp"],
    data = [":serialization_infrastructure_test_plugin.so"],
    deps = [
        ":serialization_registry",
        "@score_baselibs//score/os/mocklib:dlfcn_mock",
    ],
)
