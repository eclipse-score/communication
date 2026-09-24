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
load("@score_baselibs//:bazel/unit_tests.bzl", "cc_gtest_unit_test")
load("@score_baselibs//score/language/safecpp:toolchain_features.bzl", "COMPILER_WARNING_FEATURES")

cc_library(
    name = "shared_optional_proxy_holder",
    srcs = ["shared_optional_proxy_holder.cpp"],
    hdrs = ["shared_optional_proxy_holder.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//visibility:public"],  # platform_only
    deps = [
        "//score/mw/service:proxy_data",
        "//score/mw/service:proxy_future",
        "//score/mw/service:single_instance_holder",
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/language/safecpp/scoped_function:move_only_scoped_function",
    ],
)

cc_gtest_unit_test(
    name = "shared_optional_proxy_holder_test",
    srcs = ["shared_optional_proxy_holder_test.cpp"],
    features = COMPILER_WARNING_FEATURES + [
        "throws_upon_exception",
    ],
    visibility = ["//score/mw/service/utils:__pkg__"],
    deps = [
        ":shared_optional_proxy_holder",
        "//score/mw/service:proxy_future",
        "@score_baselibs//score/concurrency",
        "@score_baselibs//score/concurrency/future",
        "@score_baselibs//score/language/futurecpp:futurecpp_test_support",
    ],
)
