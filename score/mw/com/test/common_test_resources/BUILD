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
load("@score_baselibs//score/language/safecpp:toolchain_features.bzl", "COMPILER_WARNING_FEATURES")

cc_library(
    name = "bigdata_type",
    srcs = [
        "big_datatype.cpp",
    ],
    hdrs = [
        "big_datatype.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com/test:__subpackages__",
    ],
    deps = [
        "//score/mw/com",
    ],
)

cc_library(
    name = "sample_sender_receiver",
    srcs = [
        "sample_sender_receiver.cpp",
    ],
    hdrs = [
        "sample_sender_receiver.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com/test:__subpackages__",
    ],
    deps = [
        ":bigdata_type",
        "//score/mw/com",
        "@score_baselibs//score/os/utils/interprocess:interprocess_notification",
        "@score_logging//score/mw/log",
    ],
)

cc_library(
    name = "shared_memory_object_guard",
    srcs = [
        "shared_memory_object_guard.cpp",
    ],
    hdrs = [
        "shared_memory_object_guard.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com/test:__subpackages__",
    ],
    deps = [
        ":shared_memory_object_creator",
    ],
)

cc_library(
    name = "shared_memory_object_creator",
    srcs = [
        "shared_memory_object_creator.cpp",
    ],
    hdrs = [
        "shared_memory_object_creator.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = [
        "@score_baselibs//score/language/safecpp/string_view:null_termination_check",
    ],
    visibility = [
        "//score/mw/com/test:__subpackages__",
    ],
    deps = [
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/memory/shared",
        "@score_baselibs//score/os:fcntl",
        "@score_baselibs//score/os:mman",
        "@score_baselibs//score/os:stat",
        "@score_baselibs//score/os:unistd",
    ],
)

cc_library(
    name = "stop_token_sig_term_handler",
    srcs = [
        "stop_token_sig_term_handler.cpp",
    ],
    hdrs = [
        "stop_token_sig_term_handler.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com/test:__subpackages__",
    ],
    deps = [
        "//score/mw/com",
        "@score_baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "assert_handler",
    srcs = [
        "assert_handler.cpp",
    ],
    hdrs = [
        "assert_handler.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com/test:__subpackages__",
    ],
    deps = [
        "@score_baselibs//score/language/futurecpp",
        "@score_logging//score/mw/log",
    ],
)

cc_library(
    name = "sctf_test_runner",
    srcs = [
        "sctf_test_runner.cpp",
    ],
    hdrs = [
        "sctf_test_runner.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com/test:__subpackages__",
    ],
    deps = [
        ":stop_token_sig_term_handler",
        "//score/mw/com",
        "@boost.program_options",
    ],
)

cc_library(
    name = "sync_utils",
    srcs = [
        "sync_utils.cpp",
    ],
    hdrs = [
        "sync_utils.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com/test:__subpackages__",
    ],
    deps = [
        "@score_baselibs//score/concurrency",
        "@score_baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "test_interface",
    srcs = ["test_interface.cpp"],
    hdrs = ["test_interface.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//score/mw/com/test:__subpackages__"],
    deps = [
        "//score/mw/com",
    ],
)

cc_library(
    name = "test_error_domain",
    srcs = ["test_error_domain.cpp"],
    hdrs = ["test_error_domain.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//score/mw/com/test:__subpackages__"],
    deps = [
        "//score/mw/com",
        "@score_baselibs//score/result",
    ],
)

cc_library(
    name = "common_service",
    srcs = ["common_service.cpp"],
    hdrs = ["common_service.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//score/mw/com/test:__subpackages__"],
    deps = [
        ":test_error_domain",
        "//score/mw/com",
    ],
)

cc_library(
    name = "proxy_observer",
    srcs = ["proxy_observer.cpp"],
    hdrs = ["proxy_observer.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//score/mw/com/test:__subpackages__"],
    deps = [
        "//score/mw/com",
    ],
)

cc_library(
    name = "generic_trace_api_test_resources",
    srcs = ["generic_trace_api_test_resources.cpp"],
    hdrs = ["generic_trace_api_test_resources.h"],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = [
        "@score_baselibs//score/memory/shared",
        "@score_baselibs//score/os:fcntl",
        "@score_baselibs//score/os:mman",
        "@score_baselibs//score/os:stat",
        "@score_baselibs//score/os:unistd",
    ],
    visibility = ["//score/mw/com/test:__subpackages__"],
    deps = [
        "@googletest//:gtest",
        "@score_baselibs//score/analysis/tracing/generic_trace_library/mock:trace_library_mock",
        "@score_baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "test_resources",
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//score/mw/com/test:__subpackages__"],
    deps = [
        ":check_point_control",
        ":child_process_guard",
        ":consumer_resources",
        ":general_resources",
        ":provider_resources",
        ":timeout_supervisor",
    ],
)

cc_library(
    name = "check_point_control",
    srcs = ["check_point_control.cpp"],
    hdrs = ["check_point_control.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//score/mw/com/test:__subpackages__"],
    deps = [
        ":timeout_supervisor",
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/os/utils/interprocess:interprocess_notification",
    ],
)

cc_library(
    name = "child_process_guard",
    srcs = ["child_process_guard.cpp"],
    hdrs = ["child_process_guard.h"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        "@score_baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "timeout_supervisor",
    srcs = ["timeout_supervisor.cpp"],
    hdrs = ["timeout_supervisor.h"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        "@score_baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "general_resources",
    srcs = ["general_resources.cpp"],
    hdrs = ["general_resources.h"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":check_point_control",
        ":child_process_guard",
        ":shared_memory_object_guard",
        "@score_baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "consumer_resources",
    srcs = ["consumer_resources.cpp"],
    hdrs = ["consumer_resources.h"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":check_point_control",
        "//score/mw/com:types",
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/result",
    ],
)

cc_library(
    name = "provider_resources",
    srcs = ["provider_resources.cpp"],
    hdrs = ["provider_resources.h"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":check_point_control",
        "//score/mw/com:com_error_domain",
        "//score/mw/com:types",
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/result",
    ],
)
