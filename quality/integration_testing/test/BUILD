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

load("@rules_pkg//pkg:mappings.bzl", "pkg_attributes", "pkg_files")
load("@rules_python//python:defs.bzl", "py_test")
load("//quality/integration_testing:integration_testing.bzl", "integration_test")

cc_binary(
    name = "example-app",
    srcs = ["main.cpp"],
)

cc_binary(
    name = "example-app2",
    srcs = ["main2.cpp"],
)

pkg_files(
    name = "example-app-pkg",
    srcs = [
        ":example-app",
        ":example-app2",
    ],
    attributes = pkg_attributes(
        mode = "0777",
    ),
    prefix = "/",
)

integration_test(
    name = "test_hello_world",
    timeout = "moderate",
    srcs = [
        "test_hello_world.py",
    ],
    filesystem = ":example-app-pkg",
)

# Signal probe: evidence test for the score_itf Docker network orphan bug.
# See test_signal_probe.py for full instructions.
# Scenario A (SIGKILL): bazel test :test_signal_probe --test_timeout=5
# Scenario B (SIGTERM): bazel test :test_signal_probe  then Ctrl+C
py_test(
    name = "test_signal_probe",
    timeout = "short",
    srcs = ["test_signal_probe.py"],
)
