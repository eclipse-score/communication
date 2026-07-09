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

load("@rules_python//python:defs.bzl", "py_binary", "py_library", "py_test")

py_library(
    name = "parser",
    srcs = ["parser.py"],
)

py_test(
    name = "visibility_guard_test",
    srcs = ["visibility_guard_test.py"],
    data = [":public_targets.golden"],
    # local=True: needs source tree access to read BUILD files across all packages.
    # The test is deterministic — same source tree always produces the same result.
    local = True,
    tags = [
        "external",
        "lint",
        "no-cache",
        "no-sandbox",
    ],
    target_compatible_with = ["@platforms//os:linux"],
    deps = [":parser"],
)

py_binary(
    name = "generate_golden",
    srcs = ["generate_golden.py"],
    deps = [":parser"],
)
