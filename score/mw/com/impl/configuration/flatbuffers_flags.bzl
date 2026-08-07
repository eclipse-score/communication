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
"""Build flag gating the (experimental) FlatBuffers configuration path.

The FlatBuffers config parsing strategy is not yet wired into the runtime, so the
generated ``flatbuffer_cc_library`` and related targets are compiled only when the
flag is set. Enable with ``--config=flatbuffers`` (see .bazelrc).
"""

load("@bazel_skylib//rules:common_settings.bzl", "bool_flag")

def flatbuffers_flags(name = "flatbuffers_flags"):
    """Declares the FlatBuffers feature flag and its matching config_setting.

    Args:
        name: Unused; present to satisfy the macro naming convention.
    """
    _ = name  # buildifier: disable=unused-variable
    bool_flag(
        name = "experimental_enable_flatbuffers_configuration",
        build_setting_default = False,
        visibility = ["//score/mw/com/impl/configuration:__subpackages__"],
    )

    native.config_setting(
        name = "flatbuffers_enabled",
        flag_values = {":experimental_enable_flatbuffers_configuration": "True"},
        visibility = ["//score/mw/com/impl/configuration:__subpackages__"],
    )
