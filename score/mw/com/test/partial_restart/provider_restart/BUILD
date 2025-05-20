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

load("//bazel/tools:json_schema_validator.bzl", "validate_json_schema_test")
# disabled: load("//score/bazel/packaging:adaptive_application.bzl", "pkg_adaptive_application")
load("//score/mw:common_features.bzl", "COMPILER_WARNING_FEATURES")

validate_json_schema_test(
    name = "validate_mw_com_config_schema",
    json = "mw_com_config.json",
    schema = "//score/mw/com/impl/configuration:ara_com_config_schema",
    tags = ["lint"],
)

cc_binary(
    name = "provider_restart_application",
    srcs = [
        "provider_restart_application.cpp",
        "provider_restart_application.h",
    ],
    data = ["mw_com_config.json"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":controller",
        "//score/mw/com/test/common_test_resources:stop_token_sig_term_handler",
        "//score/mw/com/test/common_test_resources:test_resources",
       # disabled, dependency missing: "//score/mw/log",
        "//third_party/boost:program_options",
        # missing dependency: "@amp",
    ],
)

cc_library(
    name = "consumer",
    srcs = ["consumer.cpp"],
    hdrs = ["consumer.h"],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = [
        "//score/mw/com",
        "//score/mw/com/test/common_test_resources:generic_trace_api_test_resources",
        "//score/mw/com/test/common_test_resources:test_resources",
        "//score/mw/com/test/partial_restart:consumer_handle_notification_data",
        "//score/mw/com/test/partial_restart:test_datatype",
    ],
    visibility = ["//score/mw/com/test/partial_restart:__subpackages__"],
    deps = [
        "//score/mw/com/test/common_test_resources:test_resources",
    ],
)

cc_library(
    name = "provider",
    srcs = ["provider.cpp"],
    hdrs = ["provider.h"],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = [
        "//score/mw/com",
        "//score/mw/com/test/common_test_resources:generic_trace_api_test_resources",
        "//score/mw/com/test/partial_restart:test_datatype",
    ],
    visibility = ["//score/mw/com/test/partial_restart:__subpackages__"],
    deps = [
        "//score/mw/com/test/common_test_resources:test_resources",
    ],
)

cc_library(
    name = "controller",
    srcs = ["controller.cpp"],
    hdrs = ["controller.h"],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = [
        "//score/mw/com",
        "//score/mw/com/test/common_test_resources:generic_trace_api_test_resources",
        "//score/mw/com/test/common_test_resources:test_resources",
    ],
    visibility = ["//score/mw/com/test/partial_restart:__subpackages__"],
    deps = [
        ":consumer",
        ":provider",
        "//score/mw/com/test/common_test_resources:shared_memory_object_guard",
        # missing dependency: "@amp",
    ],
)

pkg_adaptive_application(
    name = "provider_restart-pkg",
    application_name = "provider_restart",
    bins = [":provider_restart_application"],
    etcs = [
        "comtrace_filter_config.json",
        "mw_com_config.json",
        "logging.json",
    ],
    visibility = [
        "//score/mw/com/test/partial_restart/provider_restart/sct:__subpackages__",
        "//score/test/mw/com:__pkg__",
    ],
)
