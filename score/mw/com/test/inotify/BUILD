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

# disabled: load("//score/bazel/packaging:adaptive_application.bzl", "pkg_adaptive_application")
load("//score/mw:common_features.bzl", "COMPILER_WARNING_FEATURES")

cc_binary(
    name = "inotify_test",
    srcs = ["inotify_test.cpp"],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    visibility = ["//score/mw/com/test/inotify:__pkg__"],
    deps = [
        "//score/lib/os/utils/inotify:inotify_instance_impl",
        "//score/mw/com/test/common_test_resources:sctf_test_runner",
    ],
)

pkg_adaptive_application(
    name = "inotify-test-pkg",
    application_name = "InotifyTestApp",
    bins = [":inotify_test"],
    visibility = [
        "//score/mw/com/test/inotify/sct:__pkg__",
        "//score/test/mw/com:__pkg__",
    ],
)
