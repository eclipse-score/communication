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

load("@score_baselibs//:bazel/unit_tests.bzl", "cc_gtest_unit_test", "cc_unit_test_suites_for_host_and_qnx")
load("//score/mw:common_features.bzl", "COMPILER_WARNING_FEATURES")

cc_library(
    name = "asil_specific_cfg",
    srcs = ["asil_specific_cfg.cpp"],
    hdrs = ["asil_specific_cfg.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
)

cc_library(
    name = "client_quality_type",
    srcs = ["client_quality_type.cpp"],
    hdrs = ["client_quality_type.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
)

cc_library(
    name = "i_message_passing_service",
    srcs = [
        "i_message_passing_service.cpp",
    ],
    hdrs = [
        "i_message_passing_service.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = ["//score/mw/com/impl/bindings/lola:__pkg__"],
    deps = [
        "//score/mw/com/impl:scoped_event_receive_handler",
        "//score/mw/com/impl/bindings/lola:element_fq_id",
        "//score/mw/com/impl/bindings/lola/methods:proxy_instance_identifier",
        "//score/mw/com/impl/bindings/lola/methods:skeleton_instance_identifier",
        "//score/mw/com/impl/configuration:quality_type",
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/language/safecpp/scoped_function:copyable_scoped_function",
        "@score_baselibs//score/result",
    ],
)

cc_library(
    name = "message_passing_service",
    srcs = ["message_passing_service.cpp"],
    hdrs = ["message_passing_service.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    deps = [
        ":i_message_passing_service",
        ":message_passing_service_instance_factory",
    ],
)

cc_library(
    name = "i_message_passing_service_instance",
    srcs = ["i_message_passing_service_instance.cpp"],
    hdrs = ["i_message_passing_service_instance.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    deps = [
        ":asil_specific_cfg",
        ":client_quality_type",
        ":i_message_passing_service",
    ],
)

cc_library(
    name = "message_passing_service_instance",
    srcs = ["message_passing_service_instance.cpp"],
    hdrs = ["message_passing_service_instance.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    deps = [
        ":asil_specific_cfg",
        ":client_quality_type",
        ":i_message_passing_service_instance",
        ":message_passing_client_cache",
        ":thread_abstraction",
        "@score_baselibs//score/concurrency:thread_pool",
        "@score_baselibs//score/os:errno_logging",
        "@score_communication//score/message_passing",
    ],
)

cc_library(
    name = "message_passing_service_instance_mock",
    testonly = True,
    srcs = ["message_passing_service_instance_mock.cpp"],
    hdrs = ["message_passing_service_instance_mock.h"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":i_message_passing_service_instance",
        "@googletest//:gtest",
    ],
)

cc_library(
    name = "i_message_passing_service_instance_factory",
    srcs = ["i_message_passing_service_instance_factory.cpp"],
    hdrs = ["i_message_passing_service_instance_factory.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    deps = [
        ":asil_specific_cfg",
        ":client_quality_type",
        ":i_message_passing_service_instance",
        "@score_baselibs//score/concurrency:executor",
        "@score_communication//score/message_passing",
    ],
)

cc_library(
    name = "message_passing_service_instance_factory",
    srcs = ["message_passing_service_instance_factory.cpp"],
    hdrs = ["message_passing_service_instance_factory.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    deps = [
        ":asil_specific_cfg",
        ":client_quality_type",
        ":i_message_passing_service_instance_factory",
        ":message_passing_service_instance",
    ],
)

cc_library(
    name = "message_passing_service_instance_factory_mock",
    testonly = True,
    srcs = ["message_passing_service_instance_factory_mock.cpp"],
    hdrs = ["message_passing_service_instance_factory_mock.h"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":i_message_passing_service_instance_factory",
        "@googletest//:gtest",
    ],
)

cc_library(
    name = "message_passing_client_cache",
    srcs = ["message_passing_client_cache.cpp"],
    hdrs = ["message_passing_client_cache.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    deps = [
        ":asil_specific_cfg",
        ":client_quality_type",
        "//score/mw/com/impl/configuration",
        "@score_communication//score/message_passing",
    ],
)

cc_library(
    name = "node_identifier_copier",
    srcs = [
        "node_identifier_copier.cpp",
    ],
    hdrs = [
        "node_identifier_copier.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    deps = [
        "//score/mw/com/impl/bindings/lola:element_fq_id",
        "@score_baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "thread_abstraction",
    srcs = [
        "thread_abstraction.cpp",
    ],
    hdrs = [
        "thread_abstraction.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
)

cc_library(
    name = "messaging",
    hdrs = [
        "thread_abstraction.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = ["//score/mw/com/impl/bindings/lola:__pkg__"],
    deps = [
        ":message_passing_client_cache",
        ":message_passing_service",
        ":message_passing_service_instance_factory",
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/mw/log",
        "@score_communication//score/message_passing",
    ],
)

cc_library(
    name = "message_passing_service_mock",
    testonly = True,
    srcs = [],
    hdrs = ["message_passing_service_mock.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//score/mw/com/impl:__subpackages__"],
    deps = [
        ":i_message_passing_service",
        "//score/mw/com/impl/bindings/lola:element_fq_id",
        "//score/mw/com/impl/configuration:quality_type",
        "@googletest//:gtest",
    ],
)

cc_library(
    name = "thread_abstraction_mock",
    testonly = True,
    srcs = ["thread_abstraction_mock.cpp"],
    hdrs = ["thread_abstraction_mock.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//score/mw/com/impl:__subpackages__"],
    deps = [
        ":thread_abstraction",
        "@googletest//:gtest",
    ],
)

cc_test(
    name = "thread_abstraction_test",
    # TODO: Investigate why this unit tests takes more than 1 minute under memcheck (Ticket-141169)
    timeout = "moderate",
    srcs = [
        "thread_abstraction_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES,
    tags = ["unit"],
    visibility = [
        "//score/mw/com/impl/bindings/lola/messaging:__pkg__",
    ],
    deps = [
        ":thread_abstraction",
        ":thread_abstraction_mock",
        "@googletest//:gtest_main",
    ],
)

cc_gtest_unit_test(
    name = "message_passing_service_test",
    srcs = ["message_passing_service_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com/impl:__subpackages__",
    ],
    deps = [
        ":message_passing_service",
        ":message_passing_service_instance_factory_mock",
        ":message_passing_service_instance_mock",
    ],
)

cc_gtest_unit_test(
    name = "node_identifier_copier_test",
    srcs = ["node_identifier_copier_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com/impl:__subpackages__",
    ],
    deps = [":node_identifier_copier"],
)

cc_gtest_unit_test(
    name = "message_passing_client_cache_test",
    srcs = ["message_passing_client_cache_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com/impl/bindings/lola/messaging:__pkg__",
    ],
    deps = [
        ":messaging",
        "@score_baselibs//score/language/futurecpp",
        "@score_communication//score/message_passing:mock",
    ],
)

cc_gtest_unit_test(
    name = "message_passing_service_instance_test",
    srcs = ["message_passing_service_instance_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com/impl:__subpackages__",
    ],
    deps = [
        ":messaging",
        "@score_baselibs//score/concurrency:executor_mock",
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/os/mocklib:unistd_mock",
        "@score_communication//score/message_passing:mock",
    ],
)

cc_unit_test_suites_for_host_and_qnx(
    name = "unit_test_suite",
    cc_unit_tests = [
        ":thread_abstraction_test",
        ":node_identifier_copier_test",
        ":message_passing_service_instance_test",
        ":message_passing_client_cache_test",
        ":message_passing_service_test",
    ],
    visibility = ["//score/mw/com/impl/bindings/lola:__pkg__"],
)
