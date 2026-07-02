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
# Stage 2: cross-VM lib memory over the ivshmem BAR (Create on one VM, Open on the other).
# Self-contained: BAR discovery + the custom TypedMemory provider live here, and the binary
# rendezvous through a handshake header in the last page of the BAR. See the .cpp files.
load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")
load("@score_baselibs//score/language/safecpp:toolchain_features.bzl", "COMPILER_WARNING_FEATURES")
load("//quality/integration_testing:integration_testing.bzl", "dual_qemu_integration_test")
load("//score/mw/com/test:pkg_application.bzl", "pkg_application")

# BAR discovery + the custom TypedMemory provider that binds a shm name to the ivshmem BAR
# physical address (shm_ctl(SHMCTL_PHYS)). This is the only piece that lets *unmodified*
# score::memory::shared map onto the BAR.
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
    visibility = ["//visibility:public"],
    deps = [
        # Umbrella target that exports the factory + resource; the concrete
        # :shared_memory_factory target is not visible to us.
        "@score_baselibs//score/memory/shared",
        # Public TypedMemory interface (transitively provides user_permission.h + errno).
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
    visibility = ["//visibility:public"],
    deps = [
        ":ivshmem_lib_memory_support",
        "@score_baselibs//score/memory/shared:offset_ptr",
        "@score_baselibs//score/memory/shared",
        # lib memory pulls in mw::log; a concrete recorder backend is needed at final link
        # (otherwise: undefined reference to CreateRecorderFactory()).
        "@score_baselibs//score/mw/log:console",
    ],
)

pkg_application(
    name = "lib_memory_ivshmem_xvm-pkg",
    app_name = "lib_memory_ivshmem_xvm",
    bin = [":lib_memory_ivshmem_xvm"],
    visibility = ["//visibility:public"],
)

dual_qemu_integration_test(
    name = "test_lib_memory_ivshmem_xvm",
    srcs = ["lib_memory_ivshmem_xvm_test.py"],
    filesystem = ":lib_memory_ivshmem_xvm-pkg",
)
