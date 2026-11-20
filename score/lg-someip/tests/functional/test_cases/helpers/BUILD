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

load("@rules_python//python:defs.bzl", "py_library")
load("@score_communication_pip//:requirements.bzl", "requirement")

package(default_visibility = ["//score/lg-someip/tests/functional:__subpackages__"])

_PIP_DEPS = [
    requirement("pytest"),
    requirement("pytest-asyncio"),
    requirement("pytest-timeout"),
    requirement("scapy"),
]

# The package someip_itf: the tester side of the suites, plus the two import shims (perf_itf, attribute_plugin) that
# let the suites run unchanged. This directory goes on sys.path, so every module is imported as someip_itf.<module>.
py_library(
    name = "someip_itf",
    srcs = glob([
        "someip_itf/**/*.py",
        "perf_itf/*.py",
    ]) + ["attribute_plugin.py"],
    imports = ["."],
    deps = _PIP_DEPS,
)
