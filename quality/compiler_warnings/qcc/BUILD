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

# QCC (QNX) features.
# QCC is GCC-based and reuses the GCC warning features. Only what differs on QNX lives here.

load("@rules_cc//cc/toolchains:feature.bzl", "cc_feature")

# QNX provides dlopen()/dlsym() in libc and ships no libdl, so no link options are needed.
# The feature is still defined so that score_communication_dynamic_loading exists in every toolchain.
cc_feature(
    name = "dynamic_loading",
    feature_name = "score_communication_dynamic_loading",
    visibility = ["//visibility:public"],
)
