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
    name = "reserving_skeleton_slots",
    timeout = "moderate",
    srcs = ["test_reserving_skeleton_slots.py"],
    filesystem = "//score/mw/com/test/reserving_skeleton_slots:reserving_skeleton_slots-pkg",
)
