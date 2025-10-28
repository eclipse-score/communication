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
load("@score_baselibs//:bazel/unit_tests.bzl", "cc_gtest_unit_test", "cc_unit_test_suites_for_host_and_qnx")
load("//score/mw:common_features.bzl", "COMPILER_WARNING_FEATURES")

cc_library(
    name = "service_discovery_client",
    srcs = ["service_discovery_client.cpp"],
    hdrs = ["service_discovery_client.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = ["//score/mw/com/impl/bindings/lola:__subpackages__"],
    deps = [
        "//score/mw/com/impl:error",
        "//score/mw/com/impl:i_service_discovery_client",
        "//score/mw/com/impl/bindings/lola/service_discovery:flag_file",
        "//score/mw/com/impl/bindings/lola/service_discovery:flag_file_crawler",
        "//score/mw/com/impl/bindings/lola/service_discovery:known_instances_container",
        "//score/mw/com/impl/bindings/lola/service_discovery:lola_service_instance_identifier",
        "//score/mw/com/impl/bindings/lola/service_discovery:quality_aware_container",
        "@score_baselibs//score/concurrency:executor",
        "@score_baselibs//score/filesystem",
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/os/utils/inotify:inotify_instance",
        "@score_baselibs//score/os/utils/inotify:inotify_instance_impl",
    ],
)

cc_gtest_unit_test(
    name = "service_discovery_client_test",
    srcs = [
        "service_discovery_client_find_service_test.cpp",
        "service_discovery_client_offer_service_test.cpp",
        "service_discovery_client_start_find_service_test.cpp",
        "service_discovery_client_stop_find_service_test.cpp",
        "service_discovery_client_stop_offer_service_test.cpp",
        "service_discovery_client_worker_thread_test.cpp",
    ],
    features = COMPILER_WARNING_FEATURES,
    target_compatible_with = select({
        "@platforms//os:qnx": ["@score_baselibs//bazel/platforms/qnx_version:qnx7"],  # TODO: Ticket-214229 QNX8 incompatible
        "//conditions:default": [],
    }),
    visibility = [
        "//score/mw/com/impl/bindings/lola/service_discovery/client:__pkg__",
    ],
    deps = [
        "//score/mw/com/impl:handle_type",
        "//score/mw/com/impl:i_service_discovery",
        "//score/mw/com/impl/bindings/lola/service_discovery/client:service_discovery_client",
        "//score/mw/com/impl/bindings/lola/service_discovery/test:destructor_notifier",
        "//score/mw/com/impl/bindings/lola/service_discovery/test:file_system_guard",
        "//score/mw/com/impl/bindings/lola/service_discovery/test:service_discovery_client_test_fixtures",
        "//score/mw/com/impl/bindings/lola/service_discovery/test:service_discovery_client_test_resources",
        "//score/mw/com/impl/configuration/test:configuration_store",
        "@score_baselibs//score/os/utils/inotify:inotify_instance_mock",
    ],
)

cc_unit_test_suites_for_host_and_qnx(
    name = "unit_test_suite",
    cc_unit_tests = [
        ":service_discovery_client_test",
    ],
    visibility = ["//score/mw/com/impl/bindings/lola/service_discovery:__pkg__"],
)
