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
load("//score/mw/com/flags:flags.bzl", "SOMEIP_ENABLED_ONLY")

# The unit tests in this package depend on the libraries below and are therefore skipped together with them if SOME/IP
# support is disabled.
cc_library(
    name = "someip_strategy",
    srcs = [
        "someip_strategy.cpp",
    ],
    hdrs = [
        "someip_array.h",
        "someip_layout.h",
        "someip_max_size.h",
        "someip_scalar.h",
        "someip_strategy.h",
        "someip_string.h",
        "someip_wire_format.h",
    ],
    tags = ["FFI"],
    target_compatible_with = SOMEIP_ENABLED_ONLY,
    visibility = ["//score/mw/com/impl:__subpackages__"],
    deps = [
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/memory:endianness",
        "@score_baselibs//score/serialization:strategy_interface",
        "@score_baselibs//score/serialization/types:length_prefixed_vector",
        "@score_baselibs//score/static_reflection_with_serialization/visitor",
    ],
)

cc_library(
    name = "someip_strategy_test_fixtures",
    testonly = True,
    hdrs = ["someip_strategy_test_fixtures.h"],
    target_compatible_with = SOMEIP_ENABLED_ONLY,
    visibility = ["//score/mw/com/impl:__subpackages__"],
    deps = [
        ":someip_strategy",
        "@googletest//:gtest",
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/serialization",
        "@score_baselibs//score/static_reflection_with_serialization/visitor",
    ],
)

cc_unit_test(
    name = "someip_strategy_test",
    srcs = ["someip_strategy_test.cpp"],
    deps = [
        ":someip_strategy",
        ":someip_strategy_test_fixtures",
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/serialization",
        "@score_baselibs//score/serialization/types:length_prefixed_vector",
        "@score_baselibs//score/static_reflection_with_serialization/visitor",
    ],
)

cc_unit_test(
    name = "someip_layout_test",
    srcs = ["someip_layout_test.cpp"],
    deps = [":someip_strategy"],
)

cc_unit_test(
    name = "someip_wire_format_test",
    srcs = ["someip_wire_format_test.cpp"],
    deps = [
        ":someip_strategy",
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/serialization",
    ],
)

cc_unit_test(
    name = "someip_scalar_test",
    srcs = ["someip_scalar_test.cpp"],
    deps = [
        ":someip_strategy",
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/serialization",
    ],
)

cc_unit_test(
    name = "someip_array_test",
    srcs = ["someip_array_test.cpp"],
    deps = [
        ":someip_strategy",
        ":someip_strategy_test_fixtures",
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/serialization",
        "@score_baselibs//score/serialization/types:length_prefixed_vector",
        "@score_baselibs//score/static_reflection_with_serialization/visitor",
    ],
)

cc_unit_test(
    name = "someip_string_test",
    srcs = ["someip_string_test.cpp"],
    deps = [
        ":someip_strategy",
        ":someip_strategy_test_fixtures",
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/serialization",
        "@score_baselibs//score/static_reflection_with_serialization/visitor",
    ],
)

cc_unit_test(
    name = "someip_max_size_test",
    srcs = ["someip_max_size_test.cpp"],
    deps = [
        ":someip_strategy",
        ":someip_strategy_test_fixtures",
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/serialization",
        "@score_baselibs//score/static_reflection_with_serialization/visitor",
    ],
)

cc_unit_test(
    name = "someip_nested_test",
    srcs = ["someip_nested_test.cpp"],
    deps = [
        ":someip_strategy",
        ":someip_strategy_test_fixtures",
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/serialization",
        "@score_baselibs//score/static_reflection_with_serialization/visitor",
    ],
)
