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

load("@rules_cc//cc:defs.bzl", "cc_library")
load("@score_baselibs//:bazel/unit_tests.bzl", "cc_gtest_unit_test")
load("@score_baselibs//third_party/itf:py_unittest_qnx_test.bzl", "py_unittest_qnx_test")

py_unittest_qnx_test(
    name = "unit_tests_qnx",
    test_cases = [
        ":client_connection_test",
        ":qnx_dispatch_test",
        ":qnx_resource_path_test",
        ":timed_command_queue_test",
        ":unix_domain_test",
    ],
    test_suites = [
        "@score_communication//score/message_passing/log:unit_test_suite_qnx",
        "@score_communication//score/message_passing/non_allocating_future:unit_test_suite_qnx",
    ],
    visibility = ["//score/message_passing:__pkg__"],
)

test_suite(
    name = "unit_tests",
    tests = [
        ":client_connection_test",
        ":qnx_resource_path_test",
        ":timed_command_queue_test",
        ":unix_domain_test",
        "@score_communication//score/message_passing/log:unit_test_suite_host",
        "@score_communication//score/message_passing/non_allocating_future:unit_test_suite_host",
    ],
    visibility = ["//score/message_passing:__pkg__"],
)

cc_library(
    name = "message_passing",
    hdrs = [
        "client_factory.h",
        "engine.h",
        "server_factory.h",
    ],
    features = [
        "treat_warnings_as_errors",
        "strict_warnings",
        "additional_warnings",
    ],
    tags = ["FFI"],
    visibility = [
        "//score/mw/com/impl:__subpackages__",
        "@score_logging//score/datarouter:__subpackages__",
        "@score_logging//score/mw/log/detail/data_router:__subpackages__",
    ],
    deps = select({
        "@platforms//os:qnx": [":message_passing_qnx_dispatch"],
        "//conditions:default": [":message_passing_unix_domain"],
    }),
)

cc_library(
    name = "common_headers",
    srcs = [
        "common_headers.cpp",
    ],
    hdrs = [
        "i_client_connection.h",
        "i_client_factory.h",
        "i_connection_handler.h",
        "i_server.h",
        "i_server_connection.h",
        "i_server_factory.h",
        "i_shared_resource_engine.h",
        "server_types.h",
        "service_protocol_config.h",
        "timed_command_queue_entry.h",
    ],
    features = [
        "treat_warnings_as_errors",
        "strict_warnings",
        "additional_warnings",
    ],
    tags = ["FFI"],
    visibility = [
        "//score/mw/com/impl:__subpackages__",
    ],
    deps = [
        "@score_baselibs//score/containers:intrusivelist",
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/os:errno",
        "@score_communication//score/message_passing/log",
    ],
)

cc_library(
    name = "message_passing_common",
    srcs = [
        "client_connection.cpp",
        "timed_command_queue.cpp",
    ],
    hdrs = [
        "client_connection.h",
        "client_server_communication.h",
        "timed_command_queue.h",
    ],
    features = [
        "treat_warnings_as_errors",
        "strict_warnings",
        "additional_warnings",
    ],
    tags = ["FFI"],
    visibility = [
        "//score/mw/com/impl:__subpackages__",
    ],
    deps = [
        ":common_headers",
        "@score_communication//score/message_passing/non_allocating_future",
    ],
)

cc_library(
    name = "message_passing_unix_domain",
    srcs = [
        "unix_domain/unix_domain_client_factory.cpp",
        "unix_domain/unix_domain_engine.cpp",
        "unix_domain/unix_domain_server.cpp",
        "unix_domain/unix_domain_server_factory.cpp",
    ],
    hdrs = [
        "unix_domain/unix_domain_client_factory.h",
        "unix_domain/unix_domain_engine.h",
        "unix_domain/unix_domain_server.h",
        "unix_domain/unix_domain_server_factory.h",
        "unix_domain/unix_domain_socket_address.h",
    ],
    features = [
        "treat_warnings_as_errors",
        "strict_warnings",
        "additional_warnings",
    ],
    visibility = [
        "//score/mw/com/impl:__subpackages__",
    ],
    deps = [
        ":message_passing_common",
        "@score_baselibs//score/os:socket",
        "@score_baselibs//score/os:sys_poll",
        "@score_baselibs//score/os:unistd",
        "@score_baselibs//score/os/utils:signal",
    ],
)

cc_library(
    name = "qnx_resource_path",
    srcs = [
        "qnx_dispatch/qnx_resource_path.cpp",
    ],
    hdrs = [
        "qnx_dispatch/qnx_resource_path.h",
    ],
    features = [
        "treat_warnings_as_errors",
        "strict_warnings",
        "additional_warnings",
    ],
    tags = ["FFI"],
    visibility = [
        "//score/mw/com/impl:__subpackages__",
    ],
    deps = [
        "@score_baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "message_passing_qnx_dispatch",
    srcs = [
        "qnx_dispatch/qnx_dispatch_client_factory.cpp",
        "qnx_dispatch/qnx_dispatch_engine.cpp",
        "qnx_dispatch/qnx_dispatch_server.cpp",
        "qnx_dispatch/qnx_dispatch_server_factory.cpp",
    ],
    hdrs = [
        "qnx_dispatch/qnx_dispatch_client_factory.h",
        "qnx_dispatch/qnx_dispatch_engine.h",
        "qnx_dispatch/qnx_dispatch_server.h",
        "qnx_dispatch/qnx_dispatch_server_factory.h",
    ],
    features = [
        "treat_warnings_as_errors",
        "strict_warnings",
        "additional_warnings",
    ],
    tags = ["FFI"],
    visibility = [
        "//score/mw/com/impl:__subpackages__",
        "@score_logging//score/datarouter:__subpackages__",
        "@score_logging//score/mw/log/detail/data_router:__subpackages__",
    ],
    deps = [
        ":message_passing_common",
        ":qnx_resource_path",
        "@score_baselibs//score/os:fcntl",
        "@score_baselibs//score/os:sys_uio",
        "@score_baselibs//score/os:unistd",
        "@score_baselibs//score/os/qnx:channel",
        "@score_baselibs//score/os/qnx:dispatch",
        "@score_baselibs//score/os/qnx:iofunc",
        "@score_baselibs//score/os/qnx:timer",
        "@score_baselibs//score/os/utils:signal",
    ],
)

cc_library(
    name = "mock",
    testonly = True,
    srcs = [
        "mock/mocks.cpp",
    ],
    hdrs = [
        "mock/client_connection_mock.h",
        "mock/client_factory_mock.h",
        "mock/server_connection_mock.h",
        "mock/server_factory_mock.h",
        "mock/server_mock.h",
        "mock/shared_resource_engine_mock.h",
    ],
    deprecation = "DEPRECATED: use @score_communication//score/message_passing:mock instead",
    features = [
        "treat_warnings_as_errors",
        "strict_warnings",
        "additional_warnings",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":common_headers",
        "@googletest//:gtest",
    ],
)

cc_gtest_unit_test(
    name = "client_connection_test",
    srcs = [
        "client_connection_test.cpp",
    ],
    features = [
        "treat_warnings_as_errors",
        "strict_warnings",
        "additional_warnings",
    ],
    deps = [
        ":message_passing_common",
        ":mock",
    ],
)

cc_gtest_unit_test(
    name = "unix_domain_test",
    srcs = [
        "unix_domain_server_test.cpp",
        "unix_domain_server_to_client_test.cpp",
    ],
    features = [
        "treat_warnings_as_errors",
        "strict_warnings",
        "additional_warnings",
    ],
    deps = [
        ":message_passing_unix_domain",
    ],
)

cc_gtest_unit_test(
    name = "qnx_dispatch_test",
    srcs = [
        "qnx_dispatch_engine_test.cpp",
        "qnx_dispatch_mocked_server_test.cpp",
        "qnx_dispatch_server_test.cpp",
        "qnx_dispatch_server_to_client_test.cpp",
        "resource_manager_fixture_base.h",
    ],
    target_compatible_with = ["@platforms//os:qnx"],
    deps = [
        ":message_passing_qnx_dispatch",
        "@score_baselibs//score/concurrency",
        "@score_baselibs//score/os/mocklib:fcntl_mock",
        "@score_baselibs//score/os/mocklib:sys_uio_mock",
        "@score_baselibs//score/os/mocklib:unistd_mock",
        "@score_baselibs//score/os/mocklib/qnx:channel_mock",
        "@score_baselibs//score/os/mocklib/qnx:dispatch_mock",
        "@score_baselibs//score/os/mocklib/qnx:iofunc_mock",
        "@score_baselibs//score/os/mocklib/qnx:timer_mock",
        "@score_baselibs//score/os/utils/mocklib:signal_mock",
    ],
)

cc_gtest_unit_test(
    name = "qnx_resource_path_test",
    srcs = [
        "qnx_resource_path_test.cpp",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":qnx_resource_path",
    ],
)

cc_gtest_unit_test(
    name = "timed_command_queue_test",
    srcs = [
        "timed_command_queue_test.cpp",
    ],
    features = [
        "treat_warnings_as_errors",
        "strict_warnings",
        "additional_warnings",
    ],
    deps = [
        ":message_passing_common",
    ],
)
