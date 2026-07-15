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
load("@rules_pkg//pkg:tar.bzl", "pkg_tar")
load("@score_baselibs//score/language/safecpp:toolchain_features.bzl", "COMPILER_WARNING_FEATURES")
load("//score/mw/com/test:pkg_application.bzl", "pkg_application")

# ---------------------------------------------------------------------------
# Shared libraries: heap checker + typed service interface
# ---------------------------------------------------------------------------

cc_library(
    name = "heap_check_lib",
    hdrs = ["heap_check/heap_check.h"],
    includes = ["."],
)

cc_library(
    name = "service_interface",
    hdrs = ["common/service_interface.h"],
    includes = ["."],
    deps = [
        "//score/mw/com",
    ],
)

# Common dependency list for all example binaries in this chapter.
_COM_DEP = [
    ":heap_check_lib",
    ":service_interface",
    "//score/mw/com",
    "@score_baselibs//score/mw/log",
]

_COM_DEP_WITH_DISCOVERY = _COM_DEP + [
    "@score_baselibs//score/concurrency:notification",
    "@score_baselibs//score/language/futurecpp",
]

# ---------------------------------------------------------------------------
# event_send_receive — basic event send and receive
# ---------------------------------------------------------------------------

cc_binary(
    name = "event_send_receive_provider",
    srcs = [
        "event_send_receive/provider.cpp",
        "event_send_receive/provider.h",
    ],
    data = ["event_send_receive/mw_com_config.json"],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    deps = _COM_DEP,
)

cc_binary(
    name = "event_send_receive_consumer",
    srcs = [
        "event_send_receive/consumer.cpp",
        "event_send_receive/consumer.h",
    ],
    data = ["event_send_receive/mw_com_config.json"],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    deps = _COM_DEP,
)

pkg_application(
    name = "event_send_receive_provider-pkg",
    app_name = "HeapFreeEventSendReceiveProvider",
    bin = [":event_send_receive_provider"],
    etc = [
        "event_send_receive/logging.json",
        "event_send_receive/mw_com_config.json",
    ],
    visibility = [
        "//score/mw/com/doc:__subpackages__",
    ],
)

pkg_tar(
    name = "event_send_receive_provider-tar",
    srcs = [":event_send_receive_provider-pkg"],
    visibility = [
        "//score/mw/com/doc:__subpackages__",
    ],
)

pkg_application(
    name = "event_send_receive_consumer-pkg",
    app_name = "HeapFreeEventSendReceiveConsumer",
    bin = [":event_send_receive_consumer"],
    etc = [
        "event_send_receive/logging.json",
        "event_send_receive/mw_com_config.json",
    ],
    visibility = [
        "//score/mw/com/doc:__subpackages__",
    ],
)

pkg_tar(
    name = "event_send_receive_consumer-tar",
    srcs = [":event_send_receive_consumer-pkg"],
    visibility = [
        "//score/mw/com/doc:__subpackages__",
    ],
)

# ---------------------------------------------------------------------------
# event_field_update — event and field update together
# ---------------------------------------------------------------------------

cc_binary(
    name = "event_field_update_provider",
    srcs = [
        "event_field_update/provider.cpp",
        "event_field_update/provider.h",
    ],
    data = ["event_field_update/mw_com_config.json"],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    deps = _COM_DEP,
)

cc_binary(
    name = "event_field_update_consumer",
    srcs = [
        "event_field_update/consumer.cpp",
        "event_field_update/consumer.h",
    ],
    data = ["event_field_update/mw_com_config.json"],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    deps = _COM_DEP,
)

pkg_application(
    name = "event_field_update_provider-pkg",
    app_name = "HeapFreeEventFieldUpdateProvider",
    bin = [":event_field_update_provider"],
    etc = [
        "event_field_update/logging.json",
        "event_field_update/mw_com_config.json",
    ],
    visibility = [
        "//score/mw/com/doc:__subpackages__",
    ],
)

pkg_tar(
    name = "event_field_update_provider-tar",
    srcs = [":event_field_update_provider-pkg"],
    visibility = [
        "//score/mw/com/doc:__subpackages__",
    ],
)

pkg_application(
    name = "event_field_update_consumer-pkg",
    app_name = "HeapFreeEventFieldUpdateConsumer",
    bin = [":event_field_update_consumer"],
    etc = [
        "event_field_update/logging.json",
        "event_field_update/mw_com_config.json",
    ],
    visibility = [
        "//score/mw/com/doc:__subpackages__",
    ],
)

pkg_tar(
    name = "event_field_update_consumer-tar",
    srcs = [":event_field_update_consumer-pkg"],
    visibility = [
        "//score/mw/com/doc:__subpackages__",
    ],
)

# ---------------------------------------------------------------------------
# start_find_service — async service discovery with StartFindService
# ---------------------------------------------------------------------------

cc_binary(
    name = "start_find_service_provider",
    srcs = [
        "start_find_service/provider.cpp",
        "start_find_service/provider.h",
    ],
    data = ["start_find_service/mw_com_config.json"],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    deps = _COM_DEP_WITH_DISCOVERY,
)

cc_binary(
    name = "start_find_service_consumer",
    srcs = [
        "start_find_service/consumer.cpp",
        "start_find_service/consumer.h",
    ],
    data = ["start_find_service/mw_com_config.json"],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    deps = _COM_DEP_WITH_DISCOVERY,
)

pkg_application(
    name = "start_find_service_provider-pkg",
    app_name = "HeapFreeStartFindServiceProvider",
    bin = [":start_find_service_provider"],
    etc = [
        "start_find_service/logging.json",
        "start_find_service/mw_com_config.json",
    ],
    visibility = [
        "//score/mw/com/doc:__subpackages__",
    ],
)

pkg_tar(
    name = "start_find_service_provider-tar",
    srcs = [":start_find_service_provider-pkg"],
    visibility = [
        "//score/mw/com/doc:__subpackages__",
    ],
)

pkg_application(
    name = "start_find_service_consumer-pkg",
    app_name = "HeapFreeStartFindServiceConsumer",
    bin = [":start_find_service_consumer"],
    etc = [
        "start_find_service/logging.json",
        "start_find_service/mw_com_config.json",
    ],
    visibility = [
        "//score/mw/com/doc:__subpackages__",
    ],
)

pkg_tar(
    name = "start_find_service_consumer-tar",
    srcs = [":start_find_service_consumer-pkg"],
    visibility = [
        "//score/mw/com/doc:__subpackages__",
    ],
)

# ---------------------------------------------------------------------------
# bidirectional_discovery — both applications offer and consume simultaneously
# ---------------------------------------------------------------------------

cc_binary(
    name = "bidirectional_discovery_application_a",
    srcs = ["bidirectional_discovery/application_a.cpp"],
    data = ["bidirectional_discovery/application_a_mw_com_config.json"],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    deps = _COM_DEP_WITH_DISCOVERY,
)

cc_binary(
    name = "bidirectional_discovery_application_b",
    srcs = ["bidirectional_discovery/application_b.cpp"],
    data = ["bidirectional_discovery/application_b_mw_com_config.json"],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    deps = _COM_DEP_WITH_DISCOVERY,
)

pkg_application(
    name = "bidirectional_discovery_application_a-pkg",
    app_name = "HeapFreeBidirectionalDiscoveryA",
    bin = [":bidirectional_discovery_application_a"],
    etc = [
        "bidirectional_discovery/application_a_logging.json",
        "bidirectional_discovery/application_a_mw_com_config.json",
    ],
    visibility = [
        "//score/mw/com/doc:__subpackages__",
    ],
)

pkg_tar(
    name = "bidirectional_discovery_application_a-tar",
    srcs = [":bidirectional_discovery_application_a-pkg"],
    visibility = [
        "//score/mw/com/doc:__subpackages__",
    ],
)

pkg_application(
    name = "bidirectional_discovery_application_b-pkg",
    app_name = "HeapFreeBidirectionalDiscoveryB",
    bin = [":bidirectional_discovery_application_b"],
    etc = [
        "bidirectional_discovery/application_b_logging.json",
        "bidirectional_discovery/application_b_mw_com_config.json",
    ],
    visibility = [
        "//score/mw/com/doc:__subpackages__",
    ],
)

pkg_tar(
    name = "bidirectional_discovery_application_b-tar",
    srcs = [":bidirectional_discovery_application_b-pkg"],
    visibility = [
        "//score/mw/com/doc:__subpackages__",
    ],
)

# ---------------------------------------------------------------------------
# chapter_12 — collects all distributable tars (mirrors chapters 1-11)
# ---------------------------------------------------------------------------

filegroup(
    name = "chapter_12",
    srcs = [
        ":bidirectional_discovery_application_a-tar",
        ":bidirectional_discovery_application_b-tar",
        ":event_field_update_consumer-tar",
        ":event_field_update_provider-tar",
        ":event_send_receive_consumer-tar",
        ":event_send_receive_provider-tar",
        ":start_find_service_consumer-tar",
        ":start_find_service_provider-tar",
    ],
    visibility = ["//score/mw/com/doc:__subpackages__"],
)

# ---------------------------------------------------------------------------
# doc_files — all RST + source files referenced by literalinclude directives
# ---------------------------------------------------------------------------

filegroup(
    name = "doc_files",
    srcs = glob([
        "**/*.rst",
        "**/*.cpp",
        "**/*.h",
        "**/*.json",
    ]),
    visibility = ["//score/mw/com/doc/tutorial:__pkg__"],
)
