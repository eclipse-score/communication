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
load("//score/os:common_features.bzl", "COMPILER_WARNING_FEATURES")

cc_binary(
    name = "permission_check",
    srcs = [
        "permission_check.cpp",
    ],
    data = [],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    visibility = [
        # "@ddad//score/memory:__subpackages__",
    ],
    deps = [
        "//score/memory/shared",
        "//score/mw/log",
        "@score-baselibs//score/language/futurecpp",
    ],
)
