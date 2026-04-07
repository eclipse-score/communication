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

load("//score/mw/com/test:pkg_application.bzl", "pkg_application")
load("@score_baselibs//score/language/safecpp:toolchain_features.bzl", "COMPILER_WARNING_FEATURES")
load("//bazel/tools:json_schema_validator.bzl", "validate_json_schema_test")

validate_json_schema_test(
    name = "validate_mw_com_config_schema",
    json = "config/mw_com_config.json",
    schema = "//score/mw/com/impl/configuration:mw_com_config_schema",
    tags = ["lint"],
)

cc_binary(
    name = "provider_restart_application",
    srcs = [
        "provider_restart_application.cpp",
        "provider_restart_application.h",
    ],
    data = ["config/mw_com_config.json"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":controller",
        "//score/mw/com/test/common_test_resources:stop_token_sig_term_handler",
        "//score/mw/com/test/common_test_resources:test_resources",
        "@boost.program_options",
        "@score_baselibs//score/language/futurecpp",
        "@score_logging//score/mw/log",
    ],
)

cc_library(
    name = "consumer",
    srcs = ["consumer.cpp"],
    hdrs = ["consumer.h"],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = [
        "//score/mw/com",
        "//score/mw/com/test/common_test_resources:fail_test",
        "//score/mw/com/test/common_test_resources:test_resources",
        "//score/mw/com/test/partial_restart/methods:consumer_handle_notification_data",
        "//score/mw/com/test/partial_restart/methods:test_method_datatype",
    ],
    visibility = ["//score/mw/com/test/partial_restart/methods:__subpackages__"],
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
        "//score/mw/com/test/partial_restart/methods:test_method_datatype",
    ],
    visibility = ["//score/mw/com/test/partial_restart/methods:__subpackages__"],
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
        "//score/mw/com/test/common_test_resources:test_resources",
    ],
    visibility = ["//score/mw/com/test/partial_restart/methods:__subpackages__"],
    deps = [
        ":consumer",
        ":provider",
        "//score/mw/com/test/common_test_resources:shared_memory_object_guard",
        "@score_baselibs//score/language/futurecpp",
    ],
)

pkg_application(
    name = "provider_restart-pkg",
    app_name = "provider_restart_methods",
    bin = [":provider_restart_application"],
    etc = [
        "config/mw_com_config.json",
        "config/logging.json",
    ],
    visibility = [
        "//score/mw/com/test/partial_restart/methods/provider_restart/integration_test:__subpackages__",
    ],
)


test_suite(
    name = "component_tests",
    tests = [
        "//score/mw/com/test/partial_restart/methods/provider_restart/integration_test:component_tests",
    ],
)
