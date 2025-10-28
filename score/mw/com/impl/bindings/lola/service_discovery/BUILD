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

package(default_visibility = [
    "//score/mw/com/impl/bindings/lola/service_discovery:__subpackages__",
])

cc_library(
    name = "flag_file",
    srcs = ["flag_file.cpp"],
    hdrs = ["flag_file.h"],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = ["@score_baselibs//score/language/futurecpp"],
    tags = ["FFI"],
    visibility = ["//score/mw/com/impl/bindings/lola:__subpackages__"],
    deps = [
        "//score/mw/com/impl:enriched_instance_identifier",
        "@score_baselibs//score/filesystem",
        "@score_baselibs//score/result",
    ],
)

cc_gtest_unit_test(
    name = "flag_file_test",
    srcs = ["flag_file_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":flag_file",
        "//score/mw/com/impl/bindings/lola/service_discovery/test:service_discovery_client_test_resources",
        "//score/mw/com/impl/configuration/test:configuration_store",
        "@score_baselibs//score/filesystem:mock",
        "@score_baselibs//score/os:unistd",
    ],
)

cc_library(
    name = "flag_file_crawler",
    srcs = ["flag_file_crawler.cpp"],
    hdrs = ["flag_file_crawler.h"],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = ["@score_baselibs//score/language/futurecpp"],
    tags = ["FFI"],
    visibility = ["//score/mw/com/impl/bindings/lola:__subpackages__"],
    deps = [
        ":flag_file",
        ":known_instances_container",
        ":quality_aware_container",
        "//score/mw/com/impl:instance_identifier",
        "@score_baselibs//score/filesystem",
        "@score_baselibs//score/os/utils/inotify:inotify_instance",
        "@score_baselibs//score/os/utils/inotify:inotify_watch_descriptor",
    ],
)

cc_gtest_unit_test(
    name = "flag_file_crawler_test",
    srcs = ["flag_file_crawler_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":flag_file_crawler",
        "//score/mw/com/impl/bindings/lola/service_discovery/test:service_discovery_client_test_resources",
        "//score/mw/com/impl/configuration/test:configuration_store",
        "@score_baselibs//score/filesystem:mock",
        "@score_baselibs//score/os/utils/inotify:inotify_instance_mock",
    ],
)

cc_library(
    name = "known_instances_container",
    srcs = ["known_instances_container.cpp"],
    hdrs = ["known_instances_container.h"],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = ["@score_baselibs//score/language/futurecpp"],
    tags = ["FFI"],
    visibility = ["//score/mw/com/impl/bindings/lola:__subpackages__"],
    deps = [
        "//score/mw/com/impl:enriched_instance_identifier",
        "//score/mw/com/impl:handle_type",
    ],
)

cc_gtest_unit_test(
    name = "known_instances_container_test",
    srcs = ["known_instances_container_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    deps = [":known_instances_container"],
)

cc_library(
    name = "lola_service_instance_identifier",
    srcs = ["lola_service_instance_identifier.cpp"],
    hdrs = ["lola_service_instance_identifier.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = ["//score/mw/com/impl/bindings/lola:__subpackages__"],
    deps = [
        "//score/mw/com/impl:enriched_instance_identifier",
        "//score/mw/com/impl/configuration",
    ],
)

cc_gtest_unit_test(
    name = "lola_service_instance_identifier_test",
    srcs = ["lola_service_instance_identifier_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    deps = [":lola_service_instance_identifier"],
)

cc_library(
    name = "quality_aware_container",
    srcs = ["quality_aware_container.cpp"],
    hdrs = ["quality_aware_container.h"],
    features = COMPILER_WARNING_FEATURES,
    tags = ["FFI"],
    visibility = ["//score/mw/com/impl/bindings/lola:__subpackages__"],
)

cc_unit_test_suites_for_host_and_qnx(
    name = "unit_test_suite",
    cc_unit_tests = [
        ":flag_file_crawler_test",
        ":flag_file_test",
        ":known_instances_container_test",
        ":lola_service_instance_identifier_test",
    ],
    test_suites_from_sub_packages = [
        "//score/mw/com/impl/bindings/lola/service_discovery/client:unit_test_suite",
    ],
    visibility = ["//score/mw/com/impl/bindings/lola:__pkg__"],
)
