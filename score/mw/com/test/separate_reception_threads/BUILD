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
    name = "separate_reception_threads",
    srcs = [
        "separate_reception_threads_application.cpp",
        "separate_reception_threads_application.h",
    ],
    data = [
        "logging.json",
        "mw_com_config.json",
    ],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        "//score/mw/com",
        "//score/mw/com/test/common_test_resources:sctf_test_runner",
        "@boost.program_options",
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/language/safecpp/scoped_function:move_only_scoped_function",
        "@score_logging//score/mw/log",
    ],
)

pkg_application(
    name = "separate_reception_threads-pkg",
    app_name = "separate_reception_threads",
    bin = [":separate_reception_threads"],
    etc = [
        "mw_com_config.json",
        "logging.json",
    ],
    visibility = [
        "//platform/aas/test/mw/com:__pkg__",
        "//score/mw/com/test/separate_reception_threads:__subpackages__",
    ],
)
