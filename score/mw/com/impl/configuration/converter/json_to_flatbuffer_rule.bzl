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

Implemented as three distinct Bazel actions so each is scheduled and cached
independently:

1. ``flatc --jsonschema`` on the ``.fbs`` -> a JSON schema listing every enum symbol.
2. The ``json_to_flatbuffer`` preprocessor (pure ``-`` -> ``_`` key/enum normalization),
   which reads that schema to know which strings are enum symbols.
3. ``flatc --binary`` on the normalized JSON -> the ``<name>.bin`` output.
"""

visibility(["public"])

def _json_to_flatbuffer_impl(ctx):
    name = ctx.label.name
    fbs = ctx.file.fbs
    src_json = ctx.file.json

    # flatc names its output after the *input stem* in the ``-o`` directory, so we control
    # the produced filenames by controlling the declared files' stems and directories.
    fbs_stem = fbs.basename[:-len(".fbs")] if fbs.basename.endswith(".fbs") else fbs.basename

    # Action 1: flatc --jsonschema -> <name>_gen/<fbs_stem>.schema.json
    schema_json = ctx.actions.declare_file("%s_gen/%s.schema.json" % (name, fbs_stem))
    ctx.actions.run(
        executable = ctx.executable._flatc,
        arguments = ["-o", schema_json.dirname, "--jsonschema", fbs.path],
        inputs = [fbs],
        outputs = [schema_json],
        mnemonic = "FlatcJsonSchema",
        progress_message = "Generating JSON schema from %s" % fbs.short_path,
    )

    # Action 2: normalize the config (pure Python) -> <name>_gen/<name>.json
    normalized_json = ctx.actions.declare_file("%s_gen/%s.json" % (name, name))
    ctx.actions.run(
        executable = ctx.executable._converter,
        arguments = [
            "--schema",
            schema_json.path,
            "--json",
            src_json.path,
            "--output",
            normalized_json.path,
        ],
        inputs = [schema_json, src_json],
        outputs = [normalized_json],
        mnemonic = "NormalizeMwComJson",
        progress_message = "Normalizing %s" % src_json.short_path,
    )

    # Action 3: flatc --binary -> <name>.bin (stem of normalized_json is <name>).
    out_bin = ctx.actions.declare_file(name + ".bin")
    ctx.actions.run(
        executable = ctx.executable._flatc,
        arguments = ["-o", out_bin.dirname, "--binary", fbs.path, normalized_json.path],
        inputs = [fbs, normalized_json],
        outputs = [out_bin],
        mnemonic = "FlatcBinary",
        progress_message = "Building FlatBuffer %s" % out_bin.short_path,
    )

    return [DefaultInfo(files = depset([out_bin]))]

json_to_flatbuffer = rule(
    implementation = _json_to_flatbuffer_impl,
    doc = "Generates ``<name>.bin`` from a public (hyphenated) JSON config via the .fbs schema.",
    attrs = {
        "json": attr.label(
            allow_single_file = [".json"],
            mandatory = True,
            doc = "The JSON configuration file (public, hyphenated format).",
        ),
        "fbs": attr.label(
            allow_single_file = [".fbs"],
            default = "//score/mw/com/impl/configuration:mw_com_config.fbs",
            doc = "The FlatBuffers schema (single source of truth).",
        ),
        "_converter": attr.label(
            default = "//score/mw/com/impl/configuration/converter:json_to_flatbuffer",
            executable = True,
            cfg = "exec",
            doc = "The json_to_flatbuffer normalization py_binary.",
        ),
        "_flatc": attr.label(
            default = "@flatbuffers//:flatc",
            executable = True,
            cfg = "exec",
            allow_files = True,
            doc = "The flatc binary.",
        ),
    },
)
