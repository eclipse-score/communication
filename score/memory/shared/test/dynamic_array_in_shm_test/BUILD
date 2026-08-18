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
load("//quality/integration_testing:integration_testing.bzl", "integration_test")
load("//score/mw/com/test:pkg_application.bzl", "pkg_application")

cc_binary(
    name = "dynamic_array_sct",
    srcs = ["dynamic_array_sct.cpp"],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        "//score/memory/shared",
        "//score/memory/shared:managed_memory_resource",
        "//score/memory/shared:polymorphic_offset_ptr_allocator",
        "@score_baselibs//score/containers:dynamic_array",
        "@score_baselibs//score/mw/log:backend_stub_testutil",
    ],
)

pkg_application(
    name = "dynamic_array_in_shm_test-pkg",
    app_name = "dynamic_array_in_shm_test",
    bin = [":dynamic_array_sct"],
)

integration_test(
    name = "dynamic_array_in_shm_test",
    srcs = ["test_dynamic_array_sct.py"],
    filesystem = ":dynamic_array_in_shm_test-pkg",
)
