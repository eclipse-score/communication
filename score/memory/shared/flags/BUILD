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

load("@bazel_skylib//rules:common_settings.bzl", "bool_flag")

bool_flag(
    name = "use_typedshmd",
    build_setting_default = True,
)

config_setting(
    name = "config_use_typedshmd",
    flag_values = {
        ":use_typedshmd": "True",
    },
    visibility = [
        "@score_baselibs//score/memory/shared/typedshm/typedshm_wrapper:__subpackages__",
    ],
)
