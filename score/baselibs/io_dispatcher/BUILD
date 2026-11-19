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
load("//quality/unit_testing:unit_testing.bzl", "cc_unit_test")

# Empty PERF_ZONE_* macros for the profiling probes the backends carry.
# Only the backends see it, so no dependent of :io_dispatcher picks up a measurement/perf.hpp.
cc_library(
    name = "perf_stub",
    hdrs = ["perf_stub/measurement/perf.hpp"],
    includes = ["perf_stub"],
)

cc_library(
    name = "io_dispatcher",
    srcs = ["src/io_dispatcher_error.cpp"] + select({
        "@platforms//os:qnx": ["src/qnx/io_dispatcher_impl.cpp"],
        "//conditions:default": ["src/linux/io_dispatcher_impl.cpp"],
    }),
    hdrs = [
        "include/score/io_dispatcher/io_dispatcher.h",
        "include/score/io_dispatcher/io_dispatcher_error.h",
        "include/score/io_dispatcher/io_dispatcher_factory.h",
        "include/score/io_dispatcher/io_dispatcher_types.h",
    ] + select({
        "@platforms//os:qnx": ["include/score/io_dispatcher/qnx/io_dispatcher_impl.h"],
        "//conditions:default": ["include/score/io_dispatcher/linux/io_dispatcher_impl.h"],
    }),
    implementation_deps = [
        ":perf_stub",
        "@score_baselibs//score/mw/log",
        "@score_baselibs//score/os:errno",
        "@score_baselibs//score/os:unistd",
    ] + select({
        "@platforms//os:qnx": [
            "@score_baselibs//score/os/qnx:channel",
            "@score_baselibs//score/os/qnx:neutrino",
        ],
        "//conditions:default": [
            "@score_baselibs//score/os:fcntl",
            "@score_baselibs//score/os:sys_poll",
        ],
    }),
    includes = ["include"],
    visibility = ["//score:__subpackages__"],
    deps = [
        "@score_baselibs//score/containers:non_relocatable_vector",
        "@score_baselibs//score/language/futurecpp",
        "@score_baselibs//score/result",
        "@score_baselibs//score/result:error",
    ],
)

# Socket pairs, fd guards and polling helpers for the tests of this package and of the lg-someip Multiplexer.
cc_library(
    name = "test_utils",
    testonly = True,
    hdrs = ["tests/test_utils.h"],
    includes = ["tests"],
    visibility = ["//score/lg-someip/tests:__pkg__"],
    deps = [":io_dispatcher"],
)

# Cases that hold on every backend, together with those written against the backend being built.
cc_unit_test(
    name = "unit_test",
    srcs = glob(["tests/common/*.cpp"]) + select({
        "@platforms//os:qnx": glob([
            "tests/qnx/*.cpp",
            "tests/qnx/*.h",
        ]),
        "//conditions:default": glob(["tests/linux/*.cpp"]),
    }),
    linkopts = select({
        "@platforms//os:qnx": ["-lsocket"],
        "//conditions:default": [],
    }),
    deps = [
        ":io_dispatcher",
        ":test_utils",
    ],
)
