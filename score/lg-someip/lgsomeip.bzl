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

"""Bazel equivalents of the lgsomeip_configure_target() CMake helper."""

load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library", "cc_test")

visibility(["//score/lg-someip/..."])

LGSOMEIP_VERSION = "0.9.0"

_COMMON_DEFINES = [
    "APPLICATION_MGMT_ENTRY=0xF0",
    "SOMEIP_UDP_MAX_PAYLOAD_SIZE=1400",
] + select({
    "@platforms//os:qnx": [
        "QNX",
        "ENABLE_QNX_MESSAGE_PASSING",
    ],
    "//conditions:default": ["LINUX"],
}) + select({
    "//score/lg-someip:someip_tp_enabled": ["ENABLE_SOMEIP_TP"],
    "//conditions:default": [],
}) + select({
    "//score/lg-someip:someip_packet_filtering_enabled": ["ENABLE_SOMEIP_PACKET_FILTERING"],
    "//conditions:default": [],
}) + select({
    "//score/lg-someip:someip_delivery_statistics_enabled": ["ENABLE_SOMEIP_DELIVERY_STATISTICS"],
    "//conditions:default": [],
}) + select({
    "//score/lg-someip:someip_ipc_enabled": ["ENABLE_SOMEIP_IPC"],
    "//conditions:default": [],
})

_COPTS = [
    "-Wall",
    # RapidJSON assigns to a const member inside a template, which GCC 14+ rejects by default.
    "-Wno-template-body",
] + select({
    "@platforms//os:qnx": [],
    "//conditions:default": ["-pthread"],
})

_LINKOPTS = select({
    "@platforms//os:qnx": ["-lsocket"],
    "//conditions:default": ["-pthread"],
})

_FEATURES = ["-score_communication_treat_warnings_as_errors"]

def _with_defaults(kwargs, link):
    kwargs["local_defines"] = kwargs.get("local_defines", []) + _COMMON_DEFINES
    kwargs["copts"] = kwargs.get("copts", []) + _COPTS

    # The imported lg-someip sources are not warning-clean yet (the CMake build uses plain -Wall).
    kwargs["features"] = kwargs.get("features", []) + _FEATURES
    if link:
        kwargs["linkopts"] = kwargs.get("linkopts", []) + _LINKOPTS
    kwargs.setdefault("visibility", ["//score/lg-someip:__subpackages__"])
    return kwargs

def lgsomeip_cc_library(**kwargs):
    cc_library(**_with_defaults(kwargs, link = False))

def lgsomeip_cc_binary(**kwargs):
    cc_binary(**_with_defaults(kwargs, link = True))

def lgsomeip_cc_test(**kwargs):
    cc_test(**_with_defaults(kwargs, link = True))
