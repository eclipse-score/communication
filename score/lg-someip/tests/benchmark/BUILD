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
load("//score/lg-someip:lgsomeip.bzl", "lgsomeip_cc_binary")

# SOME/IP send-path and RPC benchmarks of the lg-someip stack: two apps on the lgsomeip API that run on a QNX target
# (qemu or a board) beside the routing daemon, driven from the host by the pytest suites in test_cases.

package(default_visibility = ["//score/lg-someip/tests/benchmark:__subpackages__"])

exports_files(glob(["config/*.json"]))

[
    lgsomeip_cc_binary(
        name = APP,
        testonly = True,
        srcs = ["apps/{}.cpp".format(APP)],
        deps = ["//score/lg-someip/src/someip:lgsomeip"],
    )
    for APP in [
        "bench_sender",
        "rpc_client",
    ]
]
