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
load("@rules_python//python:defs.bzl", "py_library", "py_test")

py_library(
    name = "pr_state",
    srcs = [
        "pr_digest.py",
        "pr_state.py",
    ],
    imports = ["."],
)

py_test(
    name = "pr_state_test",
    srcs = ["pr_state_test.py"],
    main = "pr_state_test.py",
    deps = [":pr_state"],
)
