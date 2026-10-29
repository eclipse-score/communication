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
load("//score/lg-someip:lgsomeip.bzl", "LGSOMEIP_VERSION", "lgsomeip_cc_binary", "lgsomeip_cc_library")

# Sources compiled into the someip-daemon core instead of the client library.
exports_files([
    "packetrouter/PacketRouterHost.cpp",
    "runtime/ServiceManager.cpp",
])

# Header-only exception classes, also used by the QNX OS abstraction.
cc_library(
    name = "exception_headers",
    hdrs = glob(["exception/*.h"]),
    includes = ["."],
    visibility = ["//score/lg-someip:__subpackages__"],
)

# Private headers (and the include paths they rely on) shared by the library,
# the daemon, the tests and the vsomeip interface.
cc_library(
    name = "internal_headers",
    hdrs = glob(["**/*.h"]),
    includes = ["."],
    visibility = ["//score/lg-someip:__subpackages__"],
    deps = [
        "//score/lg-someip:api_headers",
        "//score/lg-someip/src/osabstraction",
        "//score/lg-someip/third_party/e2e_protection:vsomeip-e2e",
        "//score/lg-someip/third_party/rapidjson",
    ],
)

lgsomeip_cc_library(
    name = "lgsomeip_objects",
    srcs = glob(
        [
            "endpoint/*.cpp",
            "message/*.cpp",
            "config/*.cpp",
            "utils/**/*.cpp",
        ],
        exclude = ["endpoint/EndpointMessagePassing.cpp"],
    ) + [
        "packetrouter/PacketRouterProxy.cpp",
        "runtime/ApplicationManager.cpp",
        "runtime/EventManager.cpp",
        "runtime/LgsomeipApi.cpp",
    ] + select({
        "@platforms//os:qnx": ["endpoint/EndpointMessagePassing.cpp"],
        "//conditions:default": [],
    }),
    local_defines = ["SOMEIP_VERSION=\\\"" + LGSOMEIP_VERSION + "\\\""],
    deps = [":internal_headers"],
    # Every object file goes into the shared library, like the CMake OBJECT library.
    alwayslink = True,
)

lgsomeip_cc_binary(
    name = "liblgsomeip.so",
    linkshared = True,
    deps = [":lgsomeip_objects"],
)

# Public lg-someip shared library. Like in the CMake build, the OS abstraction
# and the E2E protection libraries are linked statically into the consumers as well.
lgsomeip_cc_library(
    name = "lgsomeip",
    srcs = [":liblgsomeip.so"],
    deps = [
        "//score/lg-someip:api_headers",
        "//score/lg-someip/src/osabstraction",
        "//score/lg-someip/third_party/e2e_protection:vsomeip-e2e",
    ],
)
