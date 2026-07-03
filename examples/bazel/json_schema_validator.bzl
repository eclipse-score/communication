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

"""
Creates "validate_json_schema_test" test rule which validates the input "json" file against its "schema".
Mirrors @score_communication//bazel/tools:json_schema_validator.bzl for the examples standalone workspace.
"""

def _impl(ctx):
    script = """
    readonly expected_failure={expected_failure}
    '{validator}' '{schema}' < '{json}'
    readonly ret=$?
    if test "$expected_failure" = true && test "$ret" -ne 0; then
        exit 0
    elif test "$expected_failure" = false && test "$ret" -eq 0; then
        exit 0
    fi
    echo "Test Failed: Json validator performed json validation of '{json}' against '{schema}' schema with exit code $ret, meanwhile rule attribute 'expected_failure' is equal to '{expected_failure}'."
    exit 1
    """.format(
        validator = ctx.attr._validator.files_to_run.executable.short_path,
        schema = ctx.file.schema.short_path,
        json = ctx.file.json.short_path,
        expected_failure = "true" if ctx.attr.expected_failure else "false",
    )

    ctx.actions.write(
        output = ctx.outputs.executable,
        content = script,
    )

    runfiles = ctx.runfiles(files = [ctx.file._validator, ctx.file.json, ctx.file.schema])
    return [DefaultInfo(runfiles = runfiles)]

validate_json_schema_test = rule(
    implementation = _impl,
    attrs = {
        "expected_failure": attr.bool(default = False),
        "json": attr.label(
            allow_single_file = True,
            mandatory = True,
        ),
        "schema": attr.label(
            allow_single_file = True,
            mandatory = True,
        ),
        "_validator": attr.label(
            default = Label("@json_schema_validator"),
            allow_single_file = True,
            executable = True,
            cfg = "exec",
        ),
    },
    test = True,
)
