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

package(
    default_visibility = ["//score/mw/com:__subpackages__"],
)

cc_library(
    name = "serializer",
    srcs = ["serializer.cpp"],
    hdrs = ["serializer.h"],
    deps = [
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/result",
    ],
)

cc_library(
    name = "serialization_helpers",
    srcs = ["serialization_helpers.cpp"],
    hdrs = ["serialization_helpers.h"],
    visibility = ["//score/mw/com/impl/bindings/someip/serialization:__subpackages__"],
    deps = ["@score_baselibs//score/language/futurecpp"],
)
