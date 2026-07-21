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
"""Bazel rule that converts an mw::com config JSON into a FlatBuffer binary.

The rule runs the ``json_to_flatbuffer`` Python tool as a build action, passing
the pinned ``@flatbuffers//:flatc`` compiler, so ``.bin`` artifacts become part
of the build graph and can be consumed by other targets (e.g. as ``data``).
"""

def _mw_com_config_to_flatbuffer_impl(ctx):
    output = ctx.actions.declare_file(ctx.attr.out)

    args = ctx.actions.args()
    args.add("--input", ctx.file.config_json.path)
    args.add("--schema", ctx.file._schema.path)
    args.add("--output", output.path)
    args.add("--flatc", ctx.executable._flatc.path)

    ctx.actions.run(
        executable = ctx.executable._tool,
        inputs = [ctx.file.config_json, ctx.file._schema],
        outputs = [output],
        tools = [ctx.executable._flatc],
        arguments = [args],
        mnemonic = "MwComConfigToFlatBuffer",
        progress_message = "Converting %s to FlatBuffer" % ctx.file.config_json.short_path,
    )

    return [DefaultInfo(files = depset([output]))]

mw_com_config_to_flatbuffer = rule(
    implementation = _mw_com_config_to_flatbuffer_impl,
    doc = "Converts an mw::com configuration JSON file into a FlatBuffer binary.",
    attrs = {
        "config_json": attr.label(
            allow_single_file = [".json"],
            mandatory = True,
            doc = "The mw::com configuration JSON file to convert.",
        ),
        "out": attr.string(
            mandatory = True,
            doc = "Name of the FlatBuffer binary to produce (e.g. 'mw_com_config.bin').",
        ),
        "_schema": attr.label(
            allow_single_file = [".fbs"],
            default = "//score/mw/com/impl/configuration:mw_com_config.fbs",
        ),
        "_tool": attr.label(
            default = "//score/mw/com/impl/configuration/converter:json_to_flatbuffer",
            executable = True,
            cfg = "exec",
        ),
        "_flatc": attr.label(
            default = "@flatbuffers//:flatc",
            executable = True,
            cfg = "exec",
        ),
    },
)
