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
    name = "subscribe_handler",
    srcs = [
        "subscribe_handler_application.cpp",
        "subscribe_handler_application.h",
    ],
    data = ["mw_com_config.json"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        "//score/mw/com",
        "//score/mw/com/test/common_test_resources:sample_sender_receiver",
        "//score/mw/com/test/common_test_resources:sctf_test_runner",
        "//score/mw/com/test/common_test_resources:shared_memory_object_guard",
        "@score_logging//score/mw/log",
    ],
)

pkg_application(
    name = "subscribe_handler-pkg",
    app_name = "subscribe_handler",
    bin = [":subscribe_handler"],
    etc = [
        "mw_com_config.json",
        "logging.json",
    ],
    visibility = [
        "//platform/aas/test/mw/com:__pkg__",
        "//score/mw/com/test/subscribe_handler:__subpackages__",
    ],
)
