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
load("@score_baselibs//score/language/safecpp:toolchain_features.bzl", "COMPILER_WARNING_FEATURES")
load("//quality/integration_testing:integration_testing.bzl", "dual_qemu_integration_test")
load("//score/mw/com/test:pkg_application.bzl", "pkg_application")

cc_library(
    name = "ivshmem_lib_memory_support",
    srcs = [
        "bar_discovery.cpp",
        "bar_typed_memory.cpp",
    ],
    hdrs = [
        "bar_discovery.h",
        "bar_typed_memory.h",
    ],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    # The provider reaches the ivshmem shared memory through pci-server (libpci).
    linkopts = select({
        "@platforms//os:qnx": ["-lpci"],
        "//conditions:default": [],
    }),
    target_compatible_with = ["@platforms//os:qnx"],
    deps = [
        # Umbrella target that exports the factory + resource; the concrete
        # :shared_memory_factory target is not visible to us.
        "@score_baselibs//score/memory/shared",
        "@score_baselibs//score/memory/shared/typedshm/typedshm_wrapper:typedmemory",
        "@score_baselibs//score/os:errno",
    ],
)

cc_binary(
    name = "lib_memory_ivshmem_xvm",
    srcs = ["lib_memory_ivshmem_xvm.cpp"],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    target_compatible_with = ["@platforms//os:qnx"],
    deps = [
        ":ivshmem_lib_memory_support",
        "@score_baselibs//score/memory/shared",
        "@score_baselibs//score/memory/shared:managed_memory_resource",
        "@score_baselibs//score/memory/shared:map",
        "@score_baselibs//score/memory/shared:vector",
        "@score_baselibs//score/mw/log:console",
    ],
)

pkg_application(
    name = "lib_memory_ivshmem_xvm-pkg",
    app_name = "lib_memory_ivshmem_xvm",
    bin = [":lib_memory_ivshmem_xvm"],
)

dual_qemu_integration_test(
    name = "test_lib_memory_ivshmem_xvm",
    srcs = ["lib_memory_ivshmem_xvm_test.py"],
    filesystem_a = ":lib_memory_ivshmem_xvm-pkg",
    filesystem_b = ":lib_memory_ivshmem_xvm-pkg",
)
