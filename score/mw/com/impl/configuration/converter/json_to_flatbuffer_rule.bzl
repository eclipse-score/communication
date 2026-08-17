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
"""Build macro turning a mw::com JSON configuration into its FlatBuffer binary.

Composed from two independently-cached steps:

1. ``_normalize_json`` (this file) -> the ``json_to_flatbuffer`` preprocessor performs the
   pure ``-`` -> ``_`` key/enum normalization, reading a checked-in schema to know which strings
   are enum symbols.
2. ``serialize_buffer`` (baselibs) -> ``flatc --binary`` on the normalized JSON -> ``<name>.bin``.

The JSON schema is kept in sync with mw_com_config.fbs via schema_drift_test.
"""

load("@score_baselibs//score/flatbuffers/bazel:tools.bzl", "serialize_buffer")

visibility(["public"])

def _normalize_json_impl(ctx):
    normalized_json = ctx.actions.declare_file(ctx.attr.output)
    ctx.actions.run(
        executable = ctx.executable._converter,
        arguments = [
            "--schema",
            ctx.file.schema.path,
            "--json",
            ctx.file.json.path,
            "--output",
            normalized_json.path,
        ],
        inputs = [ctx.file.schema, ctx.file.json],
        outputs = [normalized_json],
        mnemonic = "NormalizeMwComJson",
        progress_message = "Normalizing %s" % ctx.file.json.short_path,
    )
    return [DefaultInfo(files = depset([normalized_json]))]

_normalize_json = rule(
    implementation = _normalize_json_impl,
    doc = "Rewrites public (hyphenated) JSON keys/enums to the fbs underscore form.",
    attrs = {
        "json": attr.label(
            allow_single_file = [".json"],
            mandatory = True,
            doc = "The JSON configuration file (public, hyphenated format).",
        ),
        "schema": attr.label(
            allow_single_file = [".json"],
            mandatory = True,
            doc = "Rich JSON schema (source of enum symbols) from generate_json_schema.",
        ),
        "output": attr.string(
            mandatory = True,
            doc = "Name of the normalized JSON output file.",
        ),
        "_converter": attr.label(
            default = "//score/mw/com/impl/configuration/converter:json_to_flatbuffer",
            executable = True,
            cfg = "exec",
            doc = "The json_to_flatbuffer normalization py_binary.",
        ),
    },
)

def json_to_flatbuffer(
        name,
        json,
        fbs = "//score/mw/com/impl/configuration:mw_com_config.fbs",
        schema = "//score/mw/com/impl/configuration:mw_com_config_schema.json",
        **kwargs):
    """Generates ``<name>.bin`` from a public (hyphenated) JSON config via the .fbs schema.

    Args:
        name: Target name; the FlatBuffer binary is ``<name>.bin``.
        json: The JSON configuration file (public, hyphenated format).
        fbs: The FlatBuffers schema.
        schema: The JSON schema (kept in sync with .fbs via schema_drift_test).
        **kwargs: Standard attributes (e.g. visibility) forwarded to the final target.
    """

    # Step 1: normalize the public JSON to the fbs underscore form.
    normalized_target = name + "_normalized"
    _normalize_json(
        name = normalized_target,
        json = json,
        schema = schema,
        output = name + ".normalized.json",
    )

    # Step 2: flatc --binary (via baselibs) on the normalized JSON.
    serialize_buffer(
        name = name,
        data = ":" + normalized_target,
        schema = fbs,
        output = name + ".bin",
        **kwargs
    )
