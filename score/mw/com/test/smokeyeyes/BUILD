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

load("//bazel/tools:json_schema_validator.bzl", "validate_json_schema_test")
# disabled: load("//score/bazel/packaging:adaptive_application.bzl", "pkg_adaptive_application")
load("//score/mw:common_features.bzl", "COMPILER_WARNING_FEATURES")

cc_binary(
    name = "smokeyeyes",
    srcs = [
        "smokeyeyes.cpp",
        "smokeyeyes.h",
    ],
    data = ["logging.json"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        "//score/mw/com",
       # disabled, dependency missing: "//score/mw/log",
        "//third_party/boost:filesystem",
        "//third_party/boost:interprocess",
        "//third_party/boost:program_options",
    ],
)

validate_json_schema_test(
    name = "validate_lola_schema",
    json = "mw_com_config.json",
    schema ="//score/mw/com:config_schema",
    tags = ["lint"],
)

pkg_adaptive_application(
    name = "smokeyeyes-pkg",
    application_name = "smokeyeyes",
    bins = [":smokeyeyes"],
    etcs = [
        "mw_com_config.json",
        "logging.json",
    ],
    visibility = [
        "//score/test/mw/com:__pkg__",
    ],
)
