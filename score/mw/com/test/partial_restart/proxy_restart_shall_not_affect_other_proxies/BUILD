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

cc_binary(
    name = "proxy_restart_shall_not_affect_other_proxies",
    srcs = ["application.cpp"],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//score/mw/com/test:__pkg__"],
    deps = [
        ":consumer",
        ":provider",
        "//score/mw/com/test/common_test_resources:shared_memory_object_guard",
        "//score/mw/com/test/common_test_resources:stop_token_sig_term_handler",
        "//score/mw/com/test/common_test_resources:test_resources",
        "//third_party/boost:program_options",
        # missing dependency: "@amp",
    ],
)

cc_library(
    name = "consumer",
    srcs = ["consumer.cpp"],
    hdrs = ["consumer.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//score/mw/com/test:__pkg__"],
    deps = [
        "//score/mw/com/test/common_test_resources:stop_token_sig_term_handler",
        "//score/mw/com/test/common_test_resources:test_resources",
        "//score/mw/com/test/partial_restart:consumer_handle_notification_data",
        "//score/mw/com/test/partial_restart:test_datatype",
        # missing dependency: "@amp",
    ],
)

cc_library(
    name = "provider",
    srcs = ["provider.cpp"],
    hdrs = ["provider.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//score/mw/com/test:__pkg__"],
    deps = [
        "//score/mw/com/test/common_test_resources:stop_token_sig_term_handler",
        "//score/mw/com/test/common_test_resources:test_resources",
        "//score/mw/com/test/partial_restart:test_datatype",
        # missing dependency: "@amp",
    ],
)

validate_json_schema_test(
    name = "validate_mw_com_config_schema",
    json = "mw_com_config.json",
    schema = "//score/mw/com/impl/configuration:ara_com_config_schema",
    tags = ["unit"],
)

pkg_adaptive_application(
    name = "proxy_restart_shall_not_affect_other_proxies-pkg",
    application_name = "proxy_restart_shall_not_affect_other_proxies",
    bins = [":proxy_restart_shall_not_affect_other_proxies"],
    etcs = [
        "mw_com_config.json",
        "logging.json",
    ],
    visibility = [
        "//score/mw/com/test/partial_restart/proxy_restart_shall_not_affect_other_proxies/sct:__pkg__",
        "//score/test/mw/com:__pkg__",
    ],
)
