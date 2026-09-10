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
load("@rules_cc//cc:defs.bzl", "cc_library")
load("@score_baselibs//score/language/safecpp:toolchain_features.bzl", "COMPILER_WARNING_FEATURES")
load("//quality/unit_testing:unit_testing.bzl", "cc_unit_test")

cc_library(
    name = "qemu_hypervisor_transport",
    srcs = ["qemu_hypervisor_transport.cpp"],
    hdrs = ["qemu_hypervisor_transport.h"],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    visibility = ["//score/mw/com/gateway:__subpackages__"],
    deps = [
        "//score/memory/shared",
        "//score/mw/com/gateway/gateway_application:gateway_core",
        "//score/mw/com/gateway/transport_layer:transport",
        "//score/mw/com/gateway/transport_layer/qemu/ivshmem:ivshmem_typed_memory_provider",
        "//score/mw/com/gateway/transport_layer/sample:i_bidirectional_transport",
        "//score/mw/com/gateway/transport_layer/sample:sample_hypervisor_transport",
        "//score/mw/com/gateway/transport_layer/sample/messages:gateway_messages",
        "@score_baselibs//score/mw/log",
    ],
)

cc_unit_test(
    name = "qemu_hypervisor_transport_test",
    srcs = ["qemu_hypervisor_transport_test.cpp"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        ":qemu_hypervisor_transport",
        "//score/mw/com/gateway/gateway_application:gateway_core_mock",
        "//score/mw/com/gateway/transport_layer:transport_error",
        "//score/mw/com/gateway/transport_layer/qemu/ivshmem:ivshmem_typed_memory_provider_mock",
        "//score/mw/com/gateway/transport_layer/sample:bidirectional_transport_mock",
        "@score_baselibs//score/mw/log",
        "@score_baselibs//score/mw/log:recorder_mock",
    ] + select({
        "@platforms//os:qnx": ["@score_baselibs//score/os/mocklib/qnx:mman_mock"],
        "//conditions:default": [],
    }),
)
