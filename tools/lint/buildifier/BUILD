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

load("@rules_python//python:defs.bzl", "py_binary", "py_test")

py_binary(
    name = "buildifier_lint",
    srcs = ["buildifier_lint.py"],
    data = ["@buildifier_prebuilt//:buildifier"],
    main = "buildifier_lint.py",
    deps = ["@rules_python//python/runfiles"],
)

py_test(
    name = "buildifier_lint_test",
    srcs = [
        "buildifier_lint.py",
        "buildifier_lint_test.py",
    ],
    data = [
        "clean_fixture.bzl.txt",
        "warning_fixture.bzl.txt",
        "@buildifier_prebuilt//:buildifier",
    ],
    env = {
        "CLEAN_FIXTURE": "$(rlocationpath clean_fixture.bzl.txt)",
        "WARNING_FIXTURE": "$(rlocationpath warning_fixture.bzl.txt)",
    },
    imports = ["."],
    deps = ["@rules_python//python/runfiles"],
)
