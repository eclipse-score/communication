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
"""Helpers to hide targets behind the feature flags defined in //score/mw/com/flags."""

visibility(["//..."])

# Use as `target_compatible_with` of targets that must only be built when the FlatBuffers configuration is enabled
# (--config=flatbuffers). Targets depending on such a target have to select() it away themselves.
FLATBUFFERS_ENABLED_ONLY = select({
    "//score/mw/com/flags:flatbuffers_enabled": [],
    "//conditions:default": ["@platforms//:incompatible"],
})

def if_flatbuffers_enabled(labels):
    """Returns `labels` only if the FlatBuffers configuration is enabled (--config=flatbuffers), else an empty list.

    Use it to add dependencies on FlatBuffers targets (which are incompatible if the flag is off) to a target that is
    built regardless of the flag, e.g. `deps = [":json"] + if_flatbuffers_enabled([":flatbuffers"])`.
    """
    return select({
        "//score/mw/com/flags:flatbuffers_enabled": labels,
        "//conditions:default": [],
    })

# Use as `target_compatible_with` of targets that must only be built when SOME/IP support is enabled
# (--config=someip). Targets depending on such a target have to select() it away themselves.
SOMEIP_ENABLED_ONLY = select({
    "//score/mw/com/flags:someip_enabled": [],
    "//conditions:default": ["@platforms//:incompatible"],
})

def if_someip_enabled(labels):
    """Returns `labels` only if SOME/IP support is enabled (--config=someip), an empty list otherwise.

    Use it to add dependencies on SOME/IP targets (which are incompatible if the flag is off) to a target that is
    built regardless of the flag, e.g. `deps = [":lola"] + if_someip_enabled([":someip"])`.
    """
    return select({
        "//score/mw/com/flags:someip_enabled": labels,
        "//conditions:default": [],
    })

# Use as `target_compatible_with` of targets that must only be built when E2E support is enabled (--config=e2e).
# Targets depending on such a target have to select() it away themselves.
E2E_ENABLED_ONLY = select({
    "//score/mw/com/flags:e2e_enabled": [],
    "//conditions:default": ["@platforms//:incompatible"],
})

def if_e2e_enabled(labels):
    """Returns `labels` only if E2E support is enabled (--config=e2e), an empty list otherwise.

    Use it to add dependencies on E2E targets (which are incompatible if the flag is off) to a target that is
    built regardless of the flag, e.g. `deps = [":sample_ptr"] + if_e2e_enabled([":e2e_result"])`.
    """
    return select({
        "//score/mw/com/flags:e2e_enabled": labels,
        "//conditions:default": [],
    })
