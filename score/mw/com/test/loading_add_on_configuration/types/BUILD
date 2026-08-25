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
load("@score_baselibs//score/language/safecpp:toolchain_features.bzl", "COMPILER_WARNING_FEATURES")

cc_library(
    name = "example_interface",
    srcs = ["example_interface.cpp"],
    hdrs = ["example_interface.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//score/mw/com/test/loading_add_on_configuration:__pkg__"],
    deps = [
        "//score/mw/com",
    ],
)
