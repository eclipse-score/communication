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

load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")
load("@score_baselibs//score/language/safecpp:toolchain_features.bzl", "COMPILER_WARNING_FEATURES")
load("//score/mw/com/test:pkg_application.bzl", "pkg_application")

cc_binary(
    name = "shared_memory_storage",
    srcs = [
        "shared_memory_storage_application.cpp",
        "shared_memory_storage_application.h",
    ],
    data = ["mw_com_config.json"],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    deps = [
        ":test_resources",
        "//score/mw/com",
        "//score/mw/com/test/common_test_resources:bigdata_type",
        "//score/mw/com/test/common_test_resources:sctf_test_runner",
        "//score/mw/com/test/common_test_resources:shared_memory_object_guard",
        "@score_baselibs//score/memory/shared:pointer_arithmetic_util",
        "@score_baselibs//score/os/utils/interprocess:interprocess_notification",
    ],
)

cc_library(
    name = "test_resources",
    srcs = [
        "test_resources.cpp",
        "test_resources.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        "//score/mw/com",
        "//score/mw/com/test/common_test_resources:bigdata_type",
        "@score_baselibs//score/os/utils/interprocess:interprocess_notification",
    ],
)

pkg_application(
    name = "shared_memory_storage-pkg",
    app_name = "shared_memory_storage",
    bin = [":shared_memory_storage"],
    etc = [
        "mw_com_config.json",
        "logging.json",
    ],
    visibility = [
        "//platform/aas/test/mw/com:__pkg__",
        "//score/mw/com/test/shared_memory_storage:__subpackages__",
    ],
)
