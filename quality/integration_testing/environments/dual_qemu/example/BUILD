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
load("//quality/integration_testing:integration_testing.bzl", "dual_qemu_integration_test")
load("//score/mw/com/test:pkg_application.bzl", "pkg_application")

cc_binary(
    name = "qemu_shared_ivshmem",
    srcs = [
        "offset_list.h",
        "qemu_shared_ivshmem.cpp",
    ],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    # On QNX the program reaches the ivshmem shared memory through pci-server, which
    # requires the PCI access library.
    linkopts = select({
        "@platforms//os:qnx": ["-lpci"],
        "//conditions:default": [],
    }),
    # The mapping uses QNX pci-server + mmap_device_memory; build on qnx only.
    target_compatible_with = ["@platforms//os:qnx"],
    visibility = ["//visibility:public"],
)

pkg_application(
    name = "qemu_shared_ivshmem-pkg",
    app_name = "qemu_shared_ivshmem",
    bin = [":qemu_shared_ivshmem"],
    visibility = ["//visibility:public"],
)

dual_qemu_integration_test(
    name = "test_qemu_shared_ivshmem",
    srcs = ["qemu_shared_ivshmem_test.py"],
    filesystem = ":qemu_shared_ivshmem-pkg",
)
