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

[
    lgsomeip_cc_binary(
        name = name,
        srcs = ["%s/%s.cpp" % (folder, name)],
        deps = ["//score/lg-someip/src/someip:lgsomeip"],
    )
    for folder, names in {
        "simple-event": [
            "someip-notify",
            "someip-subscribe-1",
            "someip-subscribe-2",
        ],
        "simple-method": [
            "someip-response",
            "someip-request-1",
            "someip-request-2",
            "someip-request-wrong",
        ],
    }.items()
    for name in names
]
