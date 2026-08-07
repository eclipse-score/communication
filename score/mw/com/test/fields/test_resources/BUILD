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

cc_library(
    name = "getter_and_setter_checkers",
    srcs = ["getter_and_setter_checkers.cpp"],
    hdrs = ["getter_and_setter_checkers.h"],
    visibility = ["//score/mw/com/test/fields:__subpackages__"],
    deps = [
        "//score/mw/com/test/common_test_resources:fail_test",
    ],
)
