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
load("@doxygen//:doxygen.bzl", "doxygen")

doxygen(
    name = "generate_doxygen",
    outs = [
        "html",
        "xml",
    ],
    project_name = "com",
    visibility = ["//docs/sphinx:__pkg__"],
    deps = [
        "//score/mw/com",
    ],
)
