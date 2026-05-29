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

cc_library(
    name = "common_resources",
    srcs = ["common_resources.cpp"],
    hdrs = ["common_resources.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//score/mw/com/test/methods:__subpackages__"],
    deps = [
        "//score/mw/com",
        "//score/mw/com/test/common_test_resources:fail_test",
    ],
)

# gtodo: fix deps
cc_library(
    name = "consumer",
    srcs = ["consumer.cpp"],
    hdrs = ["consumer.h"],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = [
        ":common_resources",
        ":duplicate_signatures_datatype",
        "//score/mw/com",
        "//score/mw/com/test/common_test_resources:fail_test",
        "//score/mw/com/test/methods/methods_test_resources:process_synchronizer",
        "//score/mw/com/test/methods/methods_test_resources:proxy_container",
    ],
)

cc_library(
    name = "provider",
    srcs = ["provider.cpp"],
    hdrs = ["provider.h"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":common_resources",
        ":duplicate_signatures_datatype",
        "//score/mw/com",
        "//score/mw/com/test/common_test_resources:fail_test",
        "//score/mw/com/test/methods/methods_test_resources:process_synchronizer",
        "//score/mw/com/test/methods/methods_test_resources:skeleton_container",
    ],
)

cc_binary(
    name = "main_provider",
    srcs = ["main_provider.cpp"],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    deps = [
        ":common_resources",
        ":provider",
        "//score/mw/com",
        "//score/mw/com/test/common_test_resources:command_line_parser",
        "//score/mw/com/test/common_test_resources:fail_test",
        "//score/mw/com/test/common_test_resources:stop_token_sig_term_handler",
    ],
)

cc_binary(
    name = "main_consumer",
    srcs = ["main_consumer.cpp"],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    deps = [
        ":common_resources",
        ":consumer",
        "//score/mw/com",
        "//score/mw/com/test/common_test_resources:command_line_parser",
        "//score/mw/com/test/common_test_resources:fail_test",
    ],
)

cc_binary(
    name = "main_combined_consumer_provider",
    srcs = ["main_combined_consumer_provider.cpp"],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    deps = [
        ":common_resources",
        ":consumer",
        ":provider",
        "//score/mw/com",
        "//score/mw/com/test/common_test_resources:command_line_parser",
        "//score/mw/com/test/common_test_resources:fail_test",
        "//score/mw/com/test/common_test_resources:stop_token_sig_term_handler",
    ],
)

cc_library(
    name = "duplicate_signatures_datatype",
    srcs = ["duplicate_signatures_datatype.cpp"],
    hdrs = ["duplicate_signatures_datatype.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//score/mw/com/test/methods:__subpackages__"],
    deps = [
        "//score/mw/com",
    ],
)

pkg_application(
    name = "provider-pkg",
    app_name = "MainProviderApp",
    bin = [":main_provider"],
    etc = [
        "config/provider_mw_com_config.json",
    ],
    visibility = [
        "//score/mw/com/test/methods/multiple_proxies/integration_test:__pkg__",
    ],
)

pkg_application(
    name = "consumer-pkg",
    app_name = "MainConsumerApp",
    bin = [":main_consumer"],
    etc = [
        "config/consumer0_mw_com_config.json",
        "config/consumer1_mw_com_config.json",
        "config/consumer2_mw_com_config.json",
        "config/consumer3_mw_com_config.json",
    ],
    visibility = [
        "//score/mw/com/test/methods/multiple_proxies/integration_test:__pkg__",
    ],
)

# pkg_application(
#     name = "consumer1-pkg",
#     app_name = "Consumer1App",
#     bin = [":main_consumer"],
#     etc = [
#         ":consumer1_mw_com_config",
#     ],
#     visibility = [
#         "//score/mw/com/test/methods/multiple_proxies/integration_test:__pkg__",
#     ],
# )
#
# pkg_application(
#     name = "consumer2-pkg",
#     app_name = "Consumer2App",
#     bin = [":main_consumer"],
#     etc = [
#         ":consumer2_mw_com_config",
#     ],
#     visibility = [
#         "//score/mw/com/test/methods/multiple_proxies/integration_test:__pkg__",
#     ],
# )
#
# pkg_application(
#     name = "consumer3-pkg",
#     app_name = "Consumer3App",
#     bin = [":main_consumer"],
#     etc = [
#         ":consumer3_mw_com_config",
#     ],
#     visibility = [
#         "//score/mw/com/test/methods/multiple_proxies/integration_test:__pkg__",
#     ],
# )

pkg_application(
    name = "combined-provider-consumer-pkg",
    app_name = "MainCombinedConsumerProviderApp",
    bin = [":main_combined_consumer_provider"],
    etc = [
        "config/combined_mw_com_config.json",
    ],
    visibility = [
        "//score/mw/com/test/methods/multiple_proxies/integration_test:__pkg__",
    ],
)
