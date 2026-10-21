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

cc_library(
    name = "someip_strategy",
    hdrs = [
        "someip_array.h",
        "someip_layout.h",
        "someip_scalar.h",
        "someip_string.h",
        "someip_wire_format.h",
    ],
    tags = ["FFI"],
    visibility = ["//score/mw/com/impl:__subpackages__"],
    deps = [
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/memory:endianness",
        "@score_baselibs//score/serialization:strategy_interface",
        "@score_baselibs//score/serialization/types:length_prefixed_vector",
        "@score_baselibs//score/static_reflection_with_serialization/visitor",
    ],
)

cc_unit_test(
    name = "someip_wire_format_test",
    srcs = ["someip_wire_format_test.cpp"],
    deps = [
        ":someip_strategy",
        "@score_baselibs//score/serialization",
        "@score_baselibs//score/language/futurecpp",
    ],
)

cc_unit_test(
    name = "someip_scalar_test",
    srcs = ["someip_scalar_test.cpp"],
    deps = [
        ":someip_strategy",
        "@score_baselibs//score/serialization",
        "@score_baselibs//score/language/futurecpp",
    ],
)

cc_unit_test(
    name = "someip_array_test",
    srcs = ["someip_array_test.cpp"],
    deps = [
        ":someip_strategy",
        ":someip_strategy_test_fixtures",
        "@score_baselibs//score/static_reflection_with_serialization/visitor",
        "@score_baselibs//score/serialization",
        "@score_baselibs//score/serialization/types:length_prefixed_vector",
        "@score_baselibs//score/language/futurecpp",
    ],
)

cc_unit_test(
    name = "someip_string_test",
    srcs = ["someip_string_test.cpp"],
    deps = [
        ":someip_strategy",
        ":someip_strategy_test_fixtures",
        "@score_baselibs//score/static_reflection_with_serialization/visitor",
        "@score_baselibs//score/serialization",
        "@score_baselibs//score/language/futurecpp",
    ],
)
