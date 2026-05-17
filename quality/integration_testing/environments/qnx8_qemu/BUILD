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

load("@rules_pkg//pkg:mappings.bzl", "pkg_files")

filegroup(
    name = "init_build",
    srcs = ["init_x86_64.build"],
    visibility = ["//:__subpackages__"],
)

exports_files(
    ["qemu_config.json"],
    visibility = ["//:__subpackages__"],
)

filegroup(
    name = "qemu_config",
    srcs = ["qemu_config.json"],
    visibility = ["//:__subpackages__"],
)

pkg_files(
    name = "qnx_config",
    srcs = ["qcrypto.conf"],
    prefix = "/etc",
    visibility = ["//:__subpackages__"],
)
