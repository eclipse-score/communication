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

load("@score_communication_pip//:requirements.bzl", "requirement")

filegroup(
    name = "init_build",
    srcs = ["init_x86_64.build"],
    visibility = ["//:__subpackages__"],
)

py_library(
    name = "qemu",
    srcs = [
        "console.py",
        "line_reader.py",
        "qemu.py",
        "qemu_runner.py",
    ],
    target_compatible_with = ["@platforms//os:linux"],
    visibility = ["//:__subpackages__"],
    deps = [
        "//quality/integration_testing:system_under_test",
        requirement("pytest"),
    ],
)
