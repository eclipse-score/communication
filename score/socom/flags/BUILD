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

load("@rules_cc//cc/toolchains:feature.bzl", "cc_feature")

cc_feature(
    name = "someip_gateway_feature",
    feature_name = "someip-gateway",
    visibility = [
        "//:__pkg__",
        "//:__subpackages__",
        "@@score_someip_gateway+//:__pkg__",
        "@@score_someip_gateway+//:__subpackages__",
    ],
)

config_setting(
    name = "someip_gateway_enabled",
    values = {"features": "someip-gateway"},
    visibility = [
        "//:__pkg__",
        "//:__subpackages__",
        "@@score_someip_gateway+//:__pkg__",
        "@@score_someip_gateway+//:__subpackages__",
    ],
)
