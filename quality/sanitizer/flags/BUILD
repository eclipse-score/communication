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

load("@bazel_skylib//lib:selects.bzl", "selects")
load("@bazel_skylib//rules:common_settings.bzl", "string_flag")

string_flag(
    name = "sanitizer",
    build_setting_default = "none",
    values = [
        "none",
        "asan_ubsan_lsan",
        "tsan",
    ],
)

config_setting(
    name = "none",
    flag_values = {":sanitizer": "none"},
    visibility = ["//:__subpackages__"],
)

config_setting(
    name = "asan_ubsan_lsan",
    flag_values = {":sanitizer": "asan_ubsan_lsan"},
    visibility = ["//:__subpackages__"],
)

config_setting(
    name = "tsan",
    flag_values = {":sanitizer": "tsan"},
    visibility = ["//:__subpackages__"],
)

selects.config_setting_group(
    name = "any_sanitizer",
    match_any = [
        ":asan_ubsan_lsan",
        ":tsan",
    ],
)
