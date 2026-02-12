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

load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")
load("@score_baselibs//score/language/safecpp:toolchain_features.bzl", "COMPILER_WARNING_FEATURES")
load("//bazel/tools:json_schema_validator.bzl", "validate_json_schema_test")
load("//score/mw/com/test:pkg_application.bzl", "pkg_application")

validate_json_schema_test(
    name = "validate_mw_com_config_schema",
    json = "mw_com_config.json",
    schema = "//score/mw/com/impl/configuration:mw_com_config_schema",
    tags = ["lint"],
)

cc_library(
    name = "test_datatype",
    srcs = ["test_datatype.cpp"],
    hdrs = ["test_datatype.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//score/mw/com/test/service_discovery_during_consumer_crash:__subpackages__"],
    deps = [
        "//score/mw/com",
    ],
)

cc_binary(
    name = "service_discovery_during_consumer_crash_application",
    srcs = [
        "service_discovery_during_consumer_crash_application.cpp",
        "service_discovery_during_consumer_crash_application.h",
    ],
    data = ["mw_com_config.json"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":consumer",
        ":provider",
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
    deps = [
        ":test_datatype",
        "//score/mw/com/test/common_test_resources:test_resources",
    ],
)

cc_library(
    name = "provider",
    srcs = ["provider.cpp"],
    hdrs = ["provider.h"],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = [
        ":test_datatype",
        "//score/mw/com",
    ],
    deps = [
        "//score/mw/com/test/common_test_resources:test_resources",
        "@score_baselibs//score/language/futurecpp",
    ],
)

pkg_application(
    name = "service_discovery_during_consumer_crash-pkg",
    app_name = "service_discovery_during_consumer_crash",
    bin = [":service_discovery_during_consumer_crash_application"],
    etc = [
        "mw_com_config.json",
        "logging.json",
    ],
    visibility = [
        "//platform/aas/test/mw/com:__pkg__",
        "//score/mw/com/test/service_discovery_during_consumer_crash:__subpackages__",
    ],
)
