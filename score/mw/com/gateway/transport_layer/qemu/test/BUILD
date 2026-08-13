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
load("@rules_cc//cc:defs.bzl", "cc_binary")
load("@score_baselibs//score/language/safecpp:toolchain_features.bzl", "COMPILER_WARNING_FEATURES")
load("//score/mw/com/test:pkg_application.bzl", "pkg_application")

cc_binary(
    name = "app1",
    srcs = ["app1_main.cpp"],
    features = COMPILER_WARNING_FEATURES + ["aborts_upon_exception"],
    target_compatible_with = ["@platforms//os:qnx"],
    deps = [
        "//score/memory/shared",
        "//score/mw/com/gateway/transport_layer/qemu:qemu_hypervisor_transport",
        "//score/mw/com/gateway/transport_layer/qemu/ivshmem:ivshmem_bar_discovery",
        "//score/mw/com/gateway/transport_layer/qemu/ivshmem:ivshmem_typed_memory_provider",
        "//score/mw/com/gateway/transport_layer/sample/messages:gateway_messages",
        "@score_baselibs//score/mw/log:console",
        "@score_baselibs//score/result",
    ],
)

cc_binary(
    name = "app2",
    srcs = ["app2_main.cpp"],
    features = COMPILER_WARNING_FEATURES + ["aborts_upon_exception"],
    target_compatible_with = ["@platforms//os:qnx"],
    deps = [
        "//score/memory/shared",
        "//score/mw/com/gateway/transport_layer/qemu:qemu_hypervisor_transport",
        "//score/mw/com/gateway/transport_layer/qemu/ivshmem:ivshmem_bar_discovery",
        "//score/mw/com/gateway/transport_layer/qemu/ivshmem:ivshmem_typed_memory_provider",
        "//score/mw/com/gateway/transport_layer/sample/messages:gateway_messages",
        "@score_baselibs//score/mw/log:console",
        "@score_baselibs//score/result",
    ],
)

pkg_application(
    name = "qemu_transport_test_apps",
    app_name = "qemu_transport_test",
    bin = [
        ":app1",
        ":app2",
    ],
    visibility = ["//score/mw/com/gateway/transport_layer/qemu/test:__subpackages__"],
)
