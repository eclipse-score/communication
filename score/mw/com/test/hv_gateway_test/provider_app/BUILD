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

load("@rules_pkg//pkg:mappings.bzl", "pkg_files")
load("//score/mw:common_features.bzl", "COMPILER_WARNING_FEATURES")
load("//score/mw/com/test:pkg_application.bzl", "pkg_application")

cc_binary(
    name = "provider_app",
    srcs = ["provider_app.cpp"],
    features = COMPILER_WARNING_FEATURES + [
        "aborts_upon_exception",
    ],
    visibility = [
        "//ecu/xpad/xpad-shared/config/ipnext/isoc/os:__subpackages__",
    ],
    deps = [
        "//score/memory/shared",
        "//score/mw/com:runtime",
        "//score/mw/com/gateway/transport_layer/qemu/ivshmem:ivshmem_bar_discovery",
        "//score/mw/com/gateway/transport_layer/qemu/ivshmem:ivshmem_typed_memory_provider",
        "//score/mw/com/test/common_test_resources:bigdata_type",
        "//score/mw/com/test/common_test_resources:stop_token_sig_term_handler",
        "//score/mw/com/test/hv_gateway_test/common:service_interface",
    ],
)

pkg_files(
    name = "etc_files",
    srcs = [
        "configs/logging.json",
        "configs/mw_com_config.json",
    ],
    strip_prefix = "configs",
)

pkg_application(
    name = "provider_app-pkg",
    app_name = "provider_app",
    bin = [":provider_app"],
    etc_deps = [":etc_files"],
    visibility = [
        "//score/mw/com/test/hv_gateway_test:__subpackages__",
    ],
)
