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
    name = "inotify_test",
    srcs = ["inotify_test.cpp"],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    visibility = ["//score/mw/com/test/inotify:__pkg__"],
    deps = [
        "//score/mw/com/test/common_test_resources:sctf_test_runner",
        "@score_baselibs//score/os/utils/inotify:inotify_instance_impl",
    ],
)

pkg_application(
    name = "inotify-test-pkg",
    app_name = "InotifyTestApp",
    bin = [":inotify_test"],
    visibility = [
        "//platform/aas/test/mw/com:__pkg__",
        "//score/mw/com/test/inotify:__subpackages__",
    ],
)
