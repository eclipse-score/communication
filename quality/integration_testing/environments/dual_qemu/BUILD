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
load("@rules_python//python:defs.bzl", "py_library")
load("@score_itf//bazel:py_itf_plugin.bzl", "py_itf_plugin")

exports_files(["dual_qemu_config.example.json"])

py_library(
    name = "dual_qemu_lib",
    srcs = [
        "__init__.py",
        "config.py",
        "dual_qemu_process.py",
        "ivshmem_qemu.py",
    ],
    imports = [".."],
    visibility = ["//visibility:public"],
    deps = [
        "@score_itf//score/itf/core/process",
        "@score_itf//score/itf/core/utils",
        "@score_itf//score/itf/plugins/qemu",
    ],
)

# ---- ITF plugin target (used by the py_itf_test symbolic macro) ----
py_itf_plugin(
    name = "dual_qemu_plugin",
    enabled_plugins = [
        "dual_qemu",
    ],
    py_library = ":dual_qemu_lib",
    visibility = ["//visibility:public"],
)
