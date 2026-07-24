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

load("//quality/integration_testing:integration_testing.bzl", "integration_test")

integration_test(
    name = "test_bidirectional_transport_communication",
    timeout = "moderate",
    srcs = [
        "test_bidirectional_transport_communication.py",
    ],
    filesystem = "//score/mw/com/gateway/transport_layer/sample/test:application-pkg",
)

integration_test(
    name = "test_bidirectional_transport_reconnect",
    timeout = "moderate",
    srcs = [
        "test_bidirectional_transport_reconnect.py",
    ],
    filesystem = "//score/mw/com/gateway/transport_layer/sample/test:application-pkg",
    # Flaky: In rare cases the reconnect seems to happen too early so that App1 does not detect it and times out waiting,
    # while App2 finishes successfully. Test will be replaced with complete integration test (not just covering
    # socket side channel communication) in the near future.
    flaky = True,
)
