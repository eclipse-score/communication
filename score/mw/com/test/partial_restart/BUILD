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

load("//score/mw:common_features.bzl", "COMPILER_WARNING_FEATURES")

cc_library(
    name = "test_datatype",
    srcs = ["test_datatype.cpp"],
    hdrs = ["test_datatype.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//score/mw/com/test/partial_restart:__subpackages__"],
    deps = [
        "//score/mw/com",
    ],
)

cc_library(
    name = "consumer_handle_notification_data",
    srcs = ["consumer_handle_notification_data.cpp"],
    hdrs = ["consumer_handle_notification_data.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//score/mw/com/test/partial_restart:__subpackages__"],
    deps = [
        ":test_datatype",
    ],
)
