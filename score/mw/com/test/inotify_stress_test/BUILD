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

# Minimal, self-contained reproducer for the inotify event-loss/reordering bug found via
# :inotify_stress_test's kBurstLoss mode. Deliberately depends on nothing but the C/C++ standard library
# and plain POSIX APIs, so it can be shared as-is (this single source file) with QNX support.
cc_binary(
    name = "inotify_event_loss",
    srcs = [
        "inotify_event_loss.cpp",
    ],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    # On QNX, inotify_init()/inotify_add_watch()/inotify_rm_watch() are provided by libfsnotify (not
    # bundled into libc), the same system library score_baselibs' os::Inotify links against
    # (see @score_baselibs//score/os:inotify's select() on @score_bazel_platforms//:qnx8_0). This is a
    # plain linker flag for a QNX system library, not a source dependency, so inotify_event_loss.cpp
    # itself remains free of any score_baselibs/application-specific dependency.
    linkopts = select({
        "@score_bazel_platforms//:qnx8_0": ["-lfsnotify"],
        "//conditions:default": [],
    }),
    visibility = ["//score/mw/com/test/inotify_stress_test:__subpackages__"],
)

pkg_application(
    name = "inotify-event-loss-pkg",
    app_name = "InotifyEventLossApp",
    bin = [":inotify_event_loss"],
    visibility = [
        "//score/mw/com/test/inotify_stress_test:__subpackages__",
    ],
)
