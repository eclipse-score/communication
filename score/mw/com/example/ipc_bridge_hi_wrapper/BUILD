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
load("@score-baselibs//score/language/safecpp:toolchain_features.bzl", "COMPILER_WARNING_FEATURES")

cc_binary(
    name = "ipc_bridge_hi_wrapper",
    srcs = [
        "main.c",
    ],
    features = COMPILER_WARNING_FEATURES,
    deps = [
        "//score/mw/com/example/ipc_bridge:ipc_bridge_cpp",
    ],
)
