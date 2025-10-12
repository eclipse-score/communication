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
load("@score-baselibs//score/language/safecpp:toolchain_features.bzl", "COMPILER_WARNING_FEATURES")

# ToDo: Ticket-213780: These targets are duplicated from mw/com/test/common_test_resources since that folder is not open
# source yet. After open sourcing the common resources this whole folder should be deleted.

cc_library(
    name = "shared_memory_object_creator",
    srcs = [
        "shared_memory_object_creator.cpp",
    ],
    hdrs = [
        "shared_memory_object_creator.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    implementation_deps = [
        "@score-baselibs//score/language/safecpp/string_view:null_termination_check",
    ],
    visibility = [
        "//score/mw/com/performance_benchmarks:__subpackages__",
    ],
    deps = [
        "@score-baselibs//score/language/futurecpp",
        "@score-baselibs//score/memory/shared",
        "@score-baselibs//score/os:fcntl",
        "@score-baselibs//score/os:mman",
        "@score-baselibs//score/os:stat",
        "@score-baselibs//score/os:unistd",
    ],
)

cc_library(
    name = "shared_memory_object_guard",
    srcs = [
        "shared_memory_object_guard.cpp",
    ],
    hdrs = [
        "shared_memory_object_guard.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com/performance_benchmarks:__subpackages__",
    ],
    deps = [
        ":shared_memory_object_creator",
    ],
)

cc_library(
    name = "stop_token_sig_term_handler",
    srcs = [
        "stop_token_sig_term_handler.cpp",
    ],
    hdrs = [
        "stop_token_sig_term_handler.h",
    ],
    features = COMPILER_WARNING_FEATURES,
    visibility = [
        "//score/mw/com/performance_benchmarks:__subpackages__",
    ],
    deps = [
        "//score/mw/com",
        "@score-baselibs//score/language/futurecpp",
    ],
)
