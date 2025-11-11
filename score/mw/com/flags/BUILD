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

load("@bazel_skylib//rules:common_settings.bzl", "string_flag")

string_flag(
    name = "tracing_library",
    build_setting_default = "real",
    values = [
        "real",
        "stub",
    ],
)

config_setting(
    name = "tracing_library_real",
    flag_values = {":tracing_library": "real"},
    visibility = [
        "//score/mw/com/impl/tracing:__subpackages__",
    ],
)

config_setting(
    name = "tracing_library_stub",
    flag_values = {":tracing_library": "stub"},
    visibility = [
        "//score/mw/com/impl/tracing:__subpackages__",
    ],
)
