load("@score_baselibs//score/language/safecpp:toolchain_features.bzl", "COMPILER_WARNING_FEATURES")
load("//bazel/tools:json_schema_validator.bzl", "validate_json_schema_test")

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

validate_json_schema_test(
    name = "validate_mw_com_config_schema",
    json = "config/mw_com_config.json",
    schema = "//score/mw/com:config_schema",
    tags = ["lint"],
)

cc_library(
    name = "test_method_datatype",
    srcs = ["test_method_datatype.cpp"],
    hdrs = ["test_method_datatype.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//visibility:private"],
    deps = [
        "//score/mw/com",
    ],
)

cc_library(
    name = "consumer_and_provider",
    srcs = ["consumer_and_provider.cpp"],
    hdrs = ["consumer_and_provider.h"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":test_method_datatype",
        "//score/mw/com",
        "//score/mw/com/test/common_test_resources:fail_test",
        "@score_baselibs//score/concurrency",
    ],
)

cc_binary(
    name = "main_stop_offer_during_call",
    srcs = ["main_stop_offer_during_call.cpp"],
    data = [
        "config/mw_com_config.json",
    ],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":consumer_and_provider",
        "//score/mw/com",
        "//score/mw/com:runtime",
        "//score/mw/com/test/common_test_resources:stop_token_sig_term_handler",
        "//score/mw/com/test/methods/methods_test_resources:common_resources",
    ],
)

pkg_application(
    name = "stop-offer-during-call-pkg",
    app_name = "StopOfferDuringCallApp",
    bin = [":main_stop_offer_during_call"],
    etc = [
        "config/mw_com_config.json",
        "config/logging.json",
    ],
    visibility = [
        "//score/mw/com/test/methods/stop_offer_during_call/integration_test:__subpackages__",
    ],
)
