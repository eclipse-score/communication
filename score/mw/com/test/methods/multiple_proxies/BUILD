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

load("@score_baselibs//score/language/safecpp:toolchain_features.bzl", "COMPILER_WARNING_FEATURES")
load("//score/mw/com/test:pkg_application.bzl", "pkg_application")
load("//score/mw/com/test/methods/methods_test_resources:config_generator_rule.bzl", "make_default_configs")

make_default_configs(
    name = "mw_com_config",
    applicationID = "12",
)

make_default_configs(
    name = "consumer0_mw_com_config",
    applicationID = "0",
)

make_default_configs(
    name = "consumer1_mw_com_config",
    applicationID = "1",
)

make_default_configs(
    name = "consumer2_mw_com_config",
    applicationID = "2",
)

make_default_configs(
    name = "consumer3_mw_com_config",
    applicationID = "3",
)

cc_library(
    name = "common_resources",
    srcs = ["common_resources.cpp"],
    hdrs = ["common_resources.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//score/mw/com/test/methods:__subpackages__"],
    deps = [
        "//score/mw/com",
        "//score/mw/com/test/common_test_resources:fail_test",
        "//score/mw/com/test/methods/methods_test_resources:common_resources",
    ],
)

cc_binary(
    name = "provider_main",
    srcs = ["provider_main.cpp"],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    deps = [
        ":common_resources",
        "//score/mw/com/test/methods/methods_test_resources:provider_runner",
    ],
)

cc_binary(
    name = "consumer_main",
    srcs = ["consumer_main.cpp"],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    deps = [
        ":common_resources",
        "//score/mw/com/test/methods/methods_test_resources:consumer_runner",
    ],
)

cc_binary(
    name = "combined_consumer_provider_main",
    srcs = ["combined_consumer_provider_main.cpp"],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    deps = [
        ":common_resources",
        "//score/mw/com/test/common_test_resources:fail_test",
        "//score/mw/com/test/methods/methods_test_resources:consumer_runner",
        "//score/mw/com/test/methods/methods_test_resources:provider_runner",
    ],
)

pkg_application(
    name = "provider-pkg",
    app_name = "ProviderApp",
    bin = [":provider_main"],
    etc = [
        ":mw_com_config",
    ],
    visibility = [
        "//score/mw/com/test/methods/multiple_proxies/integration_test:__pkg__",
    ],
)

pkg_application(
    name = "consumer0-pkg",
    app_name = "Consumer0App",
    bin = [":consumer_main"],
    etc = [
        ":consumer0_mw_com_config",
    ],
    visibility = [
        "//score/mw/com/test/methods/multiple_proxies/integration_test:__pkg__",
    ],
)

pkg_application(
    name = "consumer1-pkg",
    app_name = "Consumer1App",
    bin = [":consumer_main"],
    etc = [
        ":consumer1_mw_com_config",
    ],
    visibility = [
        "//score/mw/com/test/methods/multiple_proxies/integration_test:__pkg__",
    ],
)

pkg_application(
    name = "consumer2-pkg",
    app_name = "Consumer2App",
    bin = [":consumer_main"],
    etc = [
        ":consumer2_mw_com_config",
    ],
    visibility = [
        "//score/mw/com/test/methods/multiple_proxies/integration_test:__pkg__",
    ],
)

pkg_application(
    name = "consumer3-pkg",
    app_name = "Consumer3App",
    bin = [":consumer_main"],
    etc = [
        ":consumer3_mw_com_config",
    ],
    visibility = [
        "//score/mw/com/test/methods/multiple_proxies/integration_test:__pkg__",
    ],
)

pkg_application(
    name = "combined-provider-consumer-pkg",
    app_name = "CombinedConsumerProviderApp",
    bin = [":combined_consumer_provider_main"],
    etc = [
        ":mw_com_config",
    ],
    visibility = [
        "//score/mw/com/test/methods/multiple_proxies/integration_test:__pkg__",
    ],
)

test_suite(
    name = "component_tests",
    tests = [
        "//score/mw/com/test/methods/multiple_proxies/integration_test:component_tests",
    ],
)
