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
    name = "inotify_stress_test",
    srcs = [
        "controller.cpp",
        "controller.h",
        "inotify_stress_test.cpp",
        "inotify_stress_test_internal.h",
        "worker.cpp",
        "worker.h",
    ],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    visibility = ["//score/mw/com/test/inotify_stress_test:__subpackages__"],
    deps = [
        "//score/mw/com/test/common_test_resources:check_point_control",
        "//score/mw/com/test/common_test_resources:general_resources",
        "@boost.program_options",
        "@score_baselibs//score/filesystem",
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/mw/log",
        "@score_baselibs//score/os/utils/inotify:inotify_instance_impl",
    ],
)

pkg_application(
    name = "inotify-stress-test-pkg",
    app_name = "InotifyStressTestApp",
    bin = [":inotify_stress_test"],
    visibility = [
        "//score/mw/com/test/inotify_stress_test:__subpackages__",
    ],
)
