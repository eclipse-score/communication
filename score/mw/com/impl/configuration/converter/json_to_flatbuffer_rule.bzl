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
"""Build rule turning a mw::com JSON configuration into its FlatBuffer binary.

Drives the thin ``json_to_flatbuffer`` preprocessor (``-`` -> ``_`` key/enum
normalization) plus ``flatc --binary`` against ``mw_com_config.fbs``.
"""

def json_to_flatbuffer(
        name,
        json,
        fbs = "//score/mw/com/impl/configuration:mw_com_config.fbs",
        converter = "//score/mw/com/impl/configuration/converter:json_to_flatbuffer",
        flatc = "@flatbuffers//:flatc",
        visibility = None):
    """Generates ``<name>.bin`` from a JSON config.

    Args:
        name: Target name; the produced binary is ``<name>.bin``.
        json: The JSON configuration file (public, hyphenated format).
        fbs: The FlatBuffers schema (single source of truth).
        converter: The ``json_to_flatbuffer`` py_binary.
        flatc: The ``flatc`` binary target.
        visibility: Standard visibility.
    """
    native.genrule(
        name = name,
        srcs = [json, fbs],
        outs = [name + ".bin"],
        cmd = " ".join([
            "$(location %s)" % converter,
            "--fbs $(location %s)" % fbs,
            "--json $(location %s)" % json,
            "--output $@",
            "--flatc $(location %s)" % flatc,
        ]),
        tools = [converter, flatc],
        visibility = visibility,
    )
