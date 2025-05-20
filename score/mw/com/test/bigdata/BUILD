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
    name = "bigdata",
    srcs = [
        "big_datatype_application.cpp",
        "big_datatype_application.h",
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

# disabled, no adaptive application packager available?
#pkg_adaptive_application(
#    name = "bigdata-pkg",
#    application_name = "bigdata",
#    bins = [":bigdata"],
#    etcs = [
#        "logging.json",
#        "mw_com_config.json",
#        "//score/mw/com/test/bigdata/franca:config_files",
#    ],
#    visibility = [
#        "//score/mw/com/test/bigdata/sct:__pkg__",
#        "//score/test/mw/com:__pkg__",
#    ],
#)
