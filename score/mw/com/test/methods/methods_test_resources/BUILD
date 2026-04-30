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
load("//bazel/tools:json_schema_validator.bzl", "validate_json_schema_test")

cc_library(
    name = "proxy_container",
    srcs = ["proxy_container.cpp"],
    hdrs = ["proxy_container.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com/test/methods:__subpackages__",
    ],
    deps = [
        "//score/mw/com",
    ],
)

cc_library(
    name = "skeleton_container",
    srcs = ["skeleton_container.cpp"],
    hdrs = ["skeleton_container.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com/test/methods:__subpackages__",
    ],
    deps = [
        "//score/mw/com",
    ],
)

cc_library(
    name = "method_provider",
    srcs = ["method_provider.cpp"],
    hdrs = ["method_provider.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com/test/methods:__subpackages__",
    ],
    deps = [
        "//score/mw/com",
        "//score/mw/com/test/common_test_resources:assert_handler",
        "//score/mw/com/test/common_test_resources:sctf_test_runner",
        "@score_baselibs//score/mw/log",
        "@score_baselibs//score/result",
    ],
)

cc_library(
    name = "method_consumer",
    srcs = ["method_consumer.cpp"],
    hdrs = ["method_consumer.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com/test/methods:__subpackages__",
    ],
    deps = [
        "//score/mw/com",
        "//score/mw/com:runtime",
        "//score/mw/com/test/common_test_resources:assert_handler",
        "//score/mw/com/test/common_test_resources:sctf_test_runner",
        "//score/mw/com/test/methods/methods_test_resources:proxy_container",
        "@score_baselibs//score/mw/log",
    ],
)

cc_library(
    name = "process_synchronizer",
    srcs = ["process_synchronizer.cpp"],
    hdrs = ["process_synchronizer.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com/test/methods:__subpackages__",
    ],
    deps = [
        "//score/mw/com/test/common_test_resources:shared_memory_object_creator",
        "//score/mw/com/test/common_test_resources:shared_memory_object_guard",
        "@score_baselibs//score/mw/log",
        "@score_baselibs//score/os/utils/interprocess:interprocess_notification",
        "@score_baselibs//score/result",
    ],
)

cc_library(
    name = "common_resources",
    srcs = ["common_resources.cpp"],
    hdrs = ["common_resources.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//score/mw/com/test/methods:__subpackages__"],
    deps = ["//score/mw/com"],
)

cc_library(
    name = "test_method_datatype",
    srcs = ["test_method_datatype.cpp"],
    hdrs = ["test_method_datatype.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//score/mw/com/test/methods:__subpackages__"],
    deps = [
        "//score/mw/com",
    ],
)

cc_library(
    name = "provider_runner",
    srcs = ["provider_runner.cpp"],
    hdrs = ["provider_runner.h"],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    visibility = ["//score/mw/com/test/methods:__subpackages__"],
    deps = [
        ":common_resources",
        ":test_method_datatype",
        "//score/mw/com",
        "//score/mw/com/test/common_test_resources:command_line_parser",
        "//score/mw/com/test/common_test_resources:fail_test",
        "//score/mw/com/test/methods/methods_test_resources:process_synchronizer",
        "@score_baselibs//score/result",
    ],
)

cc_library(
    name = "consumer_runner",
    srcs = ["consumer_runner.cpp"],
    hdrs = ["consumer_runner.h"],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    visibility = ["//score/mw/com/test/methods:__subpackages__"],
    deps = [
        ":common_resources",
        ":test_method_datatype",
        "//score/mw/com",
        "//score/mw/com/test/common_test_resources:command_line_parser",
        "//score/mw/com/test/common_test_resources:fail_test",
        "//score/mw/com/test/methods/methods_test_resources:process_synchronizer",
        "//score/mw/com/test/methods/methods_test_resources:proxy_container",
    ],
)

filegroup(
    name = "base_mw_com_config_json",
    srcs = ["base_mw_com_config.json"],
    visibility = ["//score/mw/com/test/methods:__subpackages__"],
)

py_binary(
    name = "config_generator",
    srcs = ["config_generator.py"],
    data = ["base_mw_com_config_json"],
    visibility = ["//score/mw/com/test/methods:__subpackages__"],
)

validate_json_schema_test(
    name = "validate_config_schema_provider",
    json = "base_mw_com_config.json",
    schema = "//score/mw/com:config_schema",
    tags = ["lint"],
)
