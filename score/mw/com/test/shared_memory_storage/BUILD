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
        "//score/lib/memory/shared:pointer_arithmetic_util",
        "//score/lib/os/utils/interprocess:interprocess_notification",
        "//score/mw/com",
        "//score/mw/com/test/common_test_resources:bigdata_type",
        "//score/mw/com/test/common_test_resources:sctf_test_runner",
        "//score/mw/com/test/common_test_resources:shared_memory_object_guard",
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
        "//score/lib/os/utils/interprocess:interprocess_notification",
        "//score/mw/com",
        "//score/mw/com/test/common_test_resources:bigdata_type",
    ],
)

pkg_adaptive_application(
    name = "shared_memory_storage-pkg",
    application_name = "shared_memory_storage",
    bins = [":shared_memory_storage"],
    etcs = [
        "mw_com_config.json",
        "logging.json",
    ],
    visibility = [
        "//score/mw/com/test/shared_memory_storage/sct:__pkg__",
        "//score/test/mw/com:__pkg__",
    ],
)
