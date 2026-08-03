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
        "get_and_notifier_enabled_field.cpp",
        "getter_only_enabled_field.cpp",
        "notifier_only_enabled_field.cpp",
        "set_and_get_and_notifier_enabled_field.cpp",
        "set_and_get_enabled_field.cpp",
        "set_and_notifier_enabled_field.cpp",
    ],
    hdrs = [
        "get_and_notifier_enabled_field.h",
        "getter_only_enabled_field.h",
        "notifier_only_enabled_field.h",
        "set_and_get_and_notifier_enabled_field.h",
        "set_and_get_enabled_field.h",
        "set_and_notifier_enabled_field.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//score/mw/com/test/fields/set_and_notifier:__pkg__"],
    deps = [
        "//score/mw/com",
    ],
)
