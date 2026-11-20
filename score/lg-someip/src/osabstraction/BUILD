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

load("//score/lg-someip:lgsomeip.bzl", "lgsomeip_cc_library")

lgsomeip_cc_library(
    name = "osabstraction",
    srcs = glob(
        ["**/*.cpp"],
        exclude = [
            "messagepassing/*.cpp",
            # Requires OpenSSL (ENABLE_TLS), which the Bazel build does not support yet.
            "socket/SecureConnector.cpp",
        ],
    ) + select({
        "@platforms//os:qnx": [
            "messagepassing/MessagePassingSender.cpp",
            "messagepassing/MessagePassingServer.cpp",
        ],
        "//conditions:default": [],
    }),
    hdrs = glob(["**/*.h"]),
    # The Multiplexer keeps io_dispatcher behind its Backend, so lg-someip sources do not see its headers.
    implementation_deps = [
        "//score/baselibs/io_dispatcher",
        "@score_baselibs//score/os/utils:thread",
    ],
    includes = ["."],
    # MessagePassingServer.cpp (QNX only) uses the header-only exception classes.
    deps = select({
        "@platforms//os:qnx": ["//score/lg-someip/src/someip:exception_headers"],
        "//conditions:default": [],
    }),
)
