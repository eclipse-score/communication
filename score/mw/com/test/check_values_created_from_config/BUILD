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
    name = "check_values_created_from_config",
    srcs = [
        "check_values_created_from_config_application.cpp",
        "check_values_created_from_config_application.h",
    ],
    data = ["mw_com_config.json"],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    deps = [
        "//score/mw/com",
        "//score/mw/com/test/common_test_resources:sample_sender_receiver",
        "//score/mw/com/test/common_test_resources:sctf_test_runner",
        "//score/mw/com/test/common_test_resources:shared_memory_object_creator",
        "//score/mw/com/test/common_test_resources:shared_memory_object_guard",
        "@score_baselibs//score/language/futurecpp",
    ],
)

pkg_application(
    name = "check_values_created_from_config-pkg",
    app_name = "check_values_created_from_config",
    bin = [":check_values_created_from_config"],
    etc = [
        "mw_com_config.json",
        "logging.json",
    ],
    visibility = [
        "//platform/aas/test/mw/com:__pkg__",
        "//score/mw/com/test/check_values_created_from_config:__subpackages__",
    ],
)
