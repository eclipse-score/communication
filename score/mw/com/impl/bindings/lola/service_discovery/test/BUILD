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

load("//score/mw:common_features.bzl", "COMPILER_WARNING_FEATURES")

cc_library(
    name = "service_discovery_client_test_resources",
    testonly = True,
    srcs = ["service_discovery_client_test_resources.cpp"],
    hdrs = ["service_discovery_client_test_resources.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//score/mw/com/impl/bindings/lola:__subpackages__"],
    deps = [
        "//score/mw/com/impl:find_service_handle",
        "//score/mw/com/impl:find_service_handler",
        "//score/mw/com/impl:handle_type",
        "@googletest//:gtest_main",
        "@score_baselibs//score/filesystem",
        "@score_baselibs//score/language/futurecpp",
    ],
)

cc_library(
    name = "service_discovery_client_test_fixtures",
    testonly = True,
    srcs = ["service_discovery_client_test_fixtures.cpp"],
    hdrs = ["service_discovery_client_test_fixtures.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//score/mw/com/impl/bindings/lola:__subpackages__"],
    deps = [
        ":file_system_guard",
        ":service_discovery_client_test_resources",
        "//score/mw/com/impl:handle_type",
        "//score/mw/com/impl:i_service_discovery",
        "//score/mw/com/impl/bindings/lola/service_discovery/client:service_discovery_client",
        "@googletest//:gtest_main",
        "@score_baselibs//score/concurrency:executor_mock",
        "@score_baselibs//score/concurrency:long_running_threads_container",
        "@score_baselibs//score/filesystem",
        "@score_baselibs//score/filesystem:mock",
        "@score_baselibs//score/os/utils/inotify:inotify_instance_facade",
        "@score_baselibs//score/os/utils/inotify:inotify_instance_mock",
        "@score_baselibs//score/os/utils/inotify:inotify_watch_descriptor",
    ],
)

cc_library(
    name = "file_system_guard",
    testonly = True,
    srcs = ["file_system_guard.cpp"],
    hdrs = ["file_system_guard.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//score/mw/com/impl/bindings/lola:__subpackages__"],
    deps = [
        "@score_baselibs//score/filesystem",
    ],
)

cc_library(
    name = "destructor_notifier",
    testonly = True,
    srcs = ["destructor_notifier.cpp"],
    hdrs = ["destructor_notifier.h"],
    features = COMPILER_WARNING_FEATURES,
    visibility = ["//score/mw/com/impl/bindings/lola:__subpackages__"],
    deps = [
        "@score_baselibs//score/filesystem",
    ],
)
