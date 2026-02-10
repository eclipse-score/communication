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

load("@rules_cc//cc:defs.bzl", "cc_binary")
load("@score_baselibs//score/language/safecpp:toolchain_features.bzl", "COMPILER_WARNING_FEATURES")
load("//score/mw/com/test:pkg_application.bzl", "pkg_application")

cc_binary(
    name = "flock_test",
    srcs = ["flock_test.cpp"],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    visibility = ["//score/mw/com/test/flock:__pkg__"],
    deps = [
        "@score_baselibs//score/filesystem",
        "@score_baselibs//score/memory/shared/flock:exclusive_flock_mutex",
        "@score_baselibs//score/memory/shared/flock:shared_flock_mutex",
        "@score_baselibs//score/os:fcntl",
        "@score_logging//score/mw/log",
    ],
)

pkg_application(
    name = "flock-test-pkg",
    app_name = "FlockTestApp",
    bin = [":flock_test"],
    visibility = [
        "//platform/aas/test/mw/com:__pkg__",
        "//score/mw/com/test:__pkg__",
    ],
)
