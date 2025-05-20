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

load("//score/bazel/generators:middleware.bzl", "cc_middleware_com_frontend")
load("//score/bazel/generators:unit_tests.bzl", "cc_gtest_unit_test", "cc_unit_test_suites_for_host_and_qnx")
load("//score/mw:common_features.bzl", "COMPILER_WARNING_FEATURES")

cc_middleware_com_frontend(
    name = "api",
    srcs = select({
        # TODO String type not supported due to a bug in the LLVM STL for QNX: []
        "@platforms//os:qnx": [
            "SomeComponent_qnx_cp60.fcdl",
            "simple_struct_qnx_cp60.fidl",
        ],
        "//conditions:default": [
            "SomeComponent.fcdl",
            "simple_struct.fidl",
        ],
    }),
)

cc_gtest_unit_test(
    name = "generated_api_test",
    srcs = ["generated_api_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":api",
       # disabled, dependency missing: "//score/mw/log",
    ],
)

cc_unit_test_suites_for_host_and_qnx(
    name = "unit_test_suite",
    cc_unit_tests = [
        ":generated_api_test",
    ],
    visibility = ["//score/mw/com:__pkg__"],
)
