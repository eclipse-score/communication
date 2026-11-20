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
load("@score_communication_pip//:requirements.bzl", "requirement")
load("@score_tooling//:defs.bzl", "score_py_pytest")

package(default_visibility = ["//score/lg-someip/tests/benchmark:__subpackages__"])

# The offline analyzers (wire_latency, rpc_latency, pcap_reader) and the target helpers of perf_itf.
py_library(
    name = "benchmark_tools",
    testonly = True,
    srcs = glob(["perf_itf/**/*.py"]) + [
        "pcap_reader.py",
        "rpc_latency.py",
        "wire_latency.py",
    ],
    imports = ["."],
    deps = [requirement("pytest")],
)

# One run on a target, which the suites' fixtures drive: upload, process start order and teardown, the host-side
# someipy peer, and the artifacts brought home.
py_library(
    name = "someip_bench",
    testonly = True,
    srcs = glob(["someip_bench/*.py"]),
    imports = ["."],
    deps = [
        ":benchmark_tools",
        requirement("someipy"),
    ],
)

# The pairing of sends to frames, on synthetic frames; the analyzer's own --selftest runs as one of the cases.
score_py_pytest(
    name = "wire_latency_test",
    srcs = ["wire_latency_test.py"],
    deps = [":benchmark_tools"],
)

# Client processing time from synthetic call logs and captures of requests and responses.
score_py_pytest(
    name = "rpc_latency_test",
    srcs = ["rpc_latency_test.py"],
    deps = [":benchmark_tools"],
)
