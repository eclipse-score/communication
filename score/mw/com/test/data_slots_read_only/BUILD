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
    name = "data_slots_read_only",
    srcs = [
        "data_slots_read_only_application.cpp",
        "data_slots_read_only_application.h",
    ],
    data = ["mw_com_config.json"],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    deps = [
        "//score/mw/com",
        "//score/mw/com/test/common_test_resources:sample_sender_receiver",
        "//score/mw/com/test/common_test_resources:sctf_test_runner",
    ],
)

pkg_adaptive_application(
    name = "data_slots_read_only-pkg",
    application_name = "data_slots_read_only",
    bins = [":data_slots_read_only"],
    etcs = [
        "mw_com_config.json",
        "logging.json",
    ],
    visibility = [
        "//score/mw/com/test/data_slots_read_only/sct:__pkg__",
        "//score/test/mw/com:__pkg__",
    ],
)
