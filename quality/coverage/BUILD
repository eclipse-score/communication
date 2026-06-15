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

load("@rules_shell//shell:sh_binary.bzl", "sh_binary")
load("//bazel/rules:exec_in_sysroot.bzl", "exec_in_sysroot")

sh_binary(
    name = "generate_coverage_html_bin",
    srcs = ["generate_coverage_html.sh"],
)

exec_in_sysroot(
    name = "generate_coverage_html",
    executable = ":generate_coverage_html_bin",
    sysroot = select({
        "@platforms//cpu:x86_64": "@coverage_runtime_sysroot//unpack_amd64:unpack_amd64",
        "@platforms//cpu:arm64": "@coverage_runtime_sysroot//unpack_arm64:unpack_arm64",
    }),
    visibility = ["//visibility:private"],
)
