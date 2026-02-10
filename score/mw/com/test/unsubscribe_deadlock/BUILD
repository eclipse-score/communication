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
    name = "unsubscribe_deadlock",
    srcs = [
        "unsubscribe_deadlock_application.cpp",
        "unsubscribe_deadlock_application.h",
    ],
    data = ["mw_com_config.json"],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    deps = [
        "//score/mw/com",
        "//score/mw/com/test/common_test_resources:assert_handler",
        "//score/mw/com/test/common_test_resources:bigdata_type",
        "//score/mw/com/test/common_test_resources:stop_token_sig_term_handler",
        "@score_baselibs//score/concurrency:notification",
        "@score_baselibs//score/language/futurecpp",
    ],
)

pkg_application(
    name = "unsubscribe_deadlock-pkg",
    app_name = "unsubscribe_deadlock",
    bin = [":unsubscribe_deadlock"],
    etc = [
        "mw_com_config.json",
        "logging.json",
    ],
    visibility = [
        "//platform/aas/test/mw/com:__pkg__",
        "//score/mw/com/test/unsubscribe_deadlock:__subpackages__",
    ],
)
