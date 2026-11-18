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
load("//quality/unit_testing:unit_testing.bzl", "cc_unit_test")

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

# Binary contract between mw::com and a generated serialization library. Both sides depend on it: the library
# to export its descriptor and entry points, mw::com to verify them.
cc_library(
    name = "serializer_abi",
    hdrs = ["serializer_abi.h"],
    deps = [
        ":serializer",
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/result",
    ],
)

# Loads a generated serialization library with dlopen after verifying its descriptor.
# The link options for dlopen come from the toolchains (see the score_communication_dynamic_loading feature
# in //quality/compiler_warnings).
cc_library(
    name = "serializer_library",
    srcs = ["serializer_library.cpp"],
    hdrs = ["serializer_library.h"],
    deps = [
        ":serializer",
        ":serializer_abi",
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/result",
    ],
)

cc_unit_test(
    name = "serializer_abi_test",
    srcs = ["serializer_abi_test.cpp"],
    deps = [
        ":serializer_abi",
        "@score_baselibs//score/result",
    ],
)

cc_unit_test(
    name = "serializer_library_test",
    srcs = ["serializer_library_test.cpp"],
    deps = [
        ":serializer_library",
        "@score_baselibs//score/result",
    ],
)
