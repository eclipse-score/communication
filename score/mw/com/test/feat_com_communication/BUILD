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

exports_files(
    ["logging.json"],
    visibility = ["//score/mw/com/test:__subpackages__"],
)

cc_binary(
    name = "feat_com_communication_app",
    srcs = [
        "feat_com_communication_app.cpp",
    ],
    data = ["mw_com_config.json"],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    deps = [
        "//score/mw/com",
        "//score/mw/com/test/common_test_resources:assert_handler",
        "//score/mw/com/test/common_test_resources:sample_sender_receiver",
        "//score/mw/com/test/common_test_resources:sctf_test_runner",
    ],
)

pkg_application(
    name = "feat_com_communication-pkg",
    app_name = "feat_com_communication",
    bin = [":feat_com_communication_app"],
    etc = [
        "logging.json",
        "mw_com_config.json",
        "mw_com_config_recv1.json",
        "mw_com_config_recv2.json",
        "mw_com_config_recv3.json",
    ],
    visibility = [
        "//score/mw/com/test/feat_com_communication:__subpackages__",
    ],
)
