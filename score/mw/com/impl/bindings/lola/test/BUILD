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
load("@score-baselibs//bazel:unit_tests.bzl", "cc_gtest_unit_test")
load("//score/mw:common_features.bzl", "COMPILER_WARNING_FEATURES")

cc_library(
    name = "proxy_event_test_resources",
    testonly = True,
    srcs = ["proxy_event_test_resources.cpp"],
    hdrs = ["proxy_event_test_resources.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//score/mw/com/impl:__subpackages__"],
    deps = [
        "@score-baselibs//score/memory/shared:shared_memory_factory_mock",
        "@score-baselibs//score/os/mocklib:fcntl_mock",
        "@score-baselibs//score/os/mocklib:unistd_mock",
        "//score/mw/com/impl:runtime",
        "//score/mw/com/impl:runtime_mock",
        "//score/mw/com/impl:service_discovery_mock",
        "//score/mw/com/impl/bindings/lola:event",
        "//score/mw/com/impl/bindings/lola:proxy",
        "//score/mw/com/impl/bindings/lola/messaging:mock",
        "//score/mw/com/impl/bindings/lola/test_doubles",
        "@googletest//:gtest_main",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "skeleton_test_resources",
    testonly = True,
    srcs = ["skeleton_test_resources.cpp"],
    hdrs = ["skeleton_test_resources.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com/impl/bindings/lola:__subpackages__",
    ],
    deps = [
        "@score-baselibs//score/filesystem:mock",
        "@score-baselibs//score/memory/shared:shared_memory_factory_mock",
        "@score-baselibs//score/memory/shared:shared_memory_resource_heap_allocator_mock",
        "@score-baselibs//score/memory/shared:shared_memory_resource_mock",
        "@score-baselibs//score/os/mocklib:fcntl_mock",
        "@score-baselibs//score/os/mocklib:unistd_mock",
        "//score/mw/com/impl:runtime_mock",
        "//score/mw/com/impl/bindings/lola:partial_restart_path_builder_mock",
        "//score/mw/com/impl/bindings/lola:runtime_mock",
        "//score/mw/com/impl/bindings/lola:shm_path_builder_mock",
        "//score/mw/com/impl/bindings/lola:skeleton",
        "//score/mw/com/impl/bindings/mock_binding/tracing:tracing_runtime",
        "//score/mw/com/impl/configuration",
        "//score/mw/com/impl/tracing:tracing_runtime_mock",
        "@googletest//:gtest_main",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "skeleton_event_test_resources",
    testonly = True,
    srcs = ["skeleton_event_test_resources.cpp"],
    hdrs = ["skeleton_event_test_resources.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com/impl/bindings/lola:__subpackages__",
    ],
    deps = [
        ":skeleton_test_resources",
        "//score/mw/com/impl:service_discovery_mock",
        "//score/mw/com/impl/bindings/lola:event",
        "//score/mw/com/impl/bindings/lola/messaging:mock",
        "@googletest//:gtest_main",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "service_discovery_test_resources",
    testonly = True,
    srcs = ["service_discovery_test_resources.cpp"],
    hdrs = ["service_discovery_test_resources.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//score/mw/com/impl:__subpackages__"],
    deps = [
        ":skeleton_test_resources",
        "@score-baselibs//score/filesystem",
        "//score/mw/com/impl:find_service_handle",
        "//score/mw/com/impl:find_service_handler",
        "//score/mw/com/impl:handle_type",
        "@googletest//:gtest_main",
        "@score-baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "transaction_log_test_resources",
    testonly = True,
    srcs = ["transaction_log_test_resources.cpp"],
    hdrs = ["transaction_log_test_resources.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com/impl/bindings/lola:__subpackages__",
    ],
    deps = [
        ":proxy_event_test_resources",
        "//score/mw/com/impl/bindings/lola:transaction_log",
        "//score/mw/com/impl/bindings/lola:transaction_log_id",
        "//score/mw/com/impl/bindings/lola:transaction_log_rollback_executor",
        "//score/mw/com/impl/bindings/lola:transaction_log_set",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "skeleton_component_test",
    size = "small",
    timeout = "long",
    srcs = ["skeleton_component_test.cpp"],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    tags = ["unit"],
    visibility = [
        "//score/mw/com/impl/bindings/lola:__subpackages__",
    ],
    deps = [
        ":skeleton_test_resources",
        "@score-baselibs//score/language/safecpp/coverage_termination_handler",
        "@score-baselibs//score/os/mocklib:acl_mock",
        "@score-baselibs//score/os/mocklib:fcntl_mock",
        "//score/mw/com/impl:runtime",
        "//score/mw/com/impl/bindings/mock_binding",
        "@googletest//:gtest_main",
    ],
)

cc_test(
    name = "skeleton_event_component_test",
    size = "small",
    timeout = "long",
    srcs = ["skeleton_event_component_test.cpp"],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    tags = ["unit"],
    visibility = [
        "//score/mw/com/impl/bindings/lola:__subpackages__",
    ],
    deps = [
        ":skeleton_test_resources",
        "@score-baselibs//score/language/safecpp/coverage_termination_handler",
        "@score-baselibs//score/filesystem",
        "@score-baselibs//score/os/mocklib:fcntl_mock",
        "//score/mw/com/impl:runtime_mock",
        "//score/mw/com/impl:service_discovery_mock",
        "//score/mw/com/impl/bindings/lola:runtime_mock",
        "//score/mw/com/impl/bindings/lola:skeleton",
        "//score/mw/com/impl/bindings/lola/messaging:mock",
        "//score/mw/com/impl/bindings/mock_binding/tracing:tracing_runtime",
        "//score/mw/com/impl/tracing:tracing_runtime_mock",
        "@googletest//:gtest_main",
    ],
)

cc_gtest_unit_test(
    name = "proxy_component_test",
    srcs = ["proxy_component_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com/impl/bindings/lola:__subpackages__",
    ],
    deps = [
        ":proxy_event_test_resources",
        ":service_discovery_test_resources",
        "@score-baselibs//score/filesystem",
        "//score/mw/com/impl:proxy_base",
        "//score/mw/com/impl/bindings/lola:proxy",
        "//score/mw/com/impl/bindings/lola:runtime_mock",
        "//score/mw/com/impl/bindings/lola/service_discovery/client:service_discovery_client",
        "//score/mw/com/impl/bindings/lola/test_doubles",
        "//score/mw/com/impl/configuration/test:configuration_store",
    ],
)
