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

"""Feature flag gating availability of the FlatBuffers-based mw::com configuration tooling.

FlatBuffers support (the `.fbs` schema library and the JSON-to-FlatBuffer converter)
is opt-in: it is disabled by default so plain `bazel build //...` / CI runs do not
require the `flatbuffers` toolchain unless explicitly requested.

Usage in BUILD files:
    load("//score/mw/com/impl/configuration:flatbuffers_flags.bzl", "FLATBUFFERS_TARGET_COMPATIBLE_WITH", "flatbuffers_build_settings")
    flatbuffers_build_settings(visibility = ["//score/mw/com/impl/configuration:__subpackages__"])

    some_target(
        ...
        target_compatible_with = FLATBUFFERS_TARGET_COMPATIBLE_WITH,
    )

Enable at the command line with:
    bazel build --//score/mw/com/impl/configuration:enable_flatbuffers=true //...
"""

load("@bazel_skylib//rules:common_settings.bzl", "bool_flag")

# Shared `target_compatible_with` value for any target that depends on FlatBuffers.
# Defined once here (rather than repeated per-target) so all gated targets stay in
# sync; the labels are fully qualified so this resolves correctly from any package.
FLATBUFFERS_TARGET_COMPATIBLE_WITH = select({
    "//score/mw/com/impl/configuration:flatbuffers_enabled": [],
    "//conditions:default": ["@platforms//:incompatible"],
})

def flatbuffers_build_settings(name = "flatbuffers_flags", visibility = None):
    """Declares the enable_flatbuffers flag and its corresponding config_setting.

    Args:
        name: Unused; kept for symmetry with similar *_build_settings macros.
        visibility: Visibility list applied to the generated flag/config_setting targets.
    """

    # Feature flag gating FlatBuffers-based configuration tooling. Disabled by
    # default: the flatbuffers toolchain is opt-in.
    bool_flag(
        name = "enable_flatbuffers",
        build_setting_default = False,
        visibility = visibility,
    )

    native.config_setting(
        name = "flatbuffers_enabled",
        flag_values = {":enable_flatbuffers": "True"},
        visibility = visibility,
    )
