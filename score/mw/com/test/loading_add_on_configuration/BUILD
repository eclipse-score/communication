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
    name = "add_on_loading_app",
    srcs = [
        "add_on_loading_application.cpp",
        "add_on_loading_application.h",
    ],
    data = [
        "mw_com_add_on_config.json",
        "mw_com_config.json",
    ],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    deps = [
        "//score/mw/com",
        "//score/mw/com/test/common_test_resources:assert_handler",
        "//score/mw/com/test/common_test_resources:command_line_parser",
        "//score/mw/com/test/common_test_resources:sample_sender_receiver",
        "//score/mw/com/test/common_test_resources:sctf_test_runner",
        "//score/mw/com/test/common_test_resources:shared_memory_object_creator",
        "//score/mw/com/test/common_test_resources:shared_memory_object_guard",
    ],
)

pkg_application(
    name = "add_on_loading-pkg",
    app_name = "add_on_loading_app",
    bin = [":add_on_loading_app"],
    etc = [
        "logging.json",
        "mw_com_config.json",
        "mw_com_add_on_config.json",
    ],
    visibility = [
        "//platform/aas/test/mw/com:__pkg__",
        "//pqr/abc/abc-lmn/config/project/cpu/os/hardware/hypervisor:__subpackages__",
        "//score/mw/com/test/loading_add_on_configuration:__subpackages__",
    ],
)
