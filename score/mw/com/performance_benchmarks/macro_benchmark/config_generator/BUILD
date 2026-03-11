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

load("@rules_python//python:defs.bzl", "py_binary")

py_binary(
    name = "config_generator",
    srcs = [
        "__init__.py",
        "config_generator.py",
    ],
    main = "config_generator.py",
    visibility = [
        "//platform/aas/test/pas/perf_tests/tools/artificial_load:__subpackages__",
        "//score/mw/com/performance_benchmarks/macro_benchmark:__subpackages__",
    ],
    deps = [
        "//third_party/jsonschema",
    ],
)
