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

load("@rules_cc//cc:defs.bzl", "cc_library")
load("@score_baselibs//score/language/safecpp:toolchain_features.bzl", "COMPILER_WARNING_FEATURES")

cc_library(
    name = "field_datatypes",
    srcs = [
        "getter_only_enabled_field.cpp",
        "notifier_only_enabled_field.cpp",
        "setter_and_getter_enabled_field.cpp",
        "setter_only_enabled_field.cpp",
    ],
    hdrs = [
        "getter_only_enabled_field.h",
        "notifier_only_enabled_field.h",
        "setter_and_getter_enabled_field.h",
        "setter_only_enabled_field.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//score/mw/com/test/fields/getter_setter_if_available:__pkg__"],
    deps = [
        "//score/mw/com",
    ],
)
