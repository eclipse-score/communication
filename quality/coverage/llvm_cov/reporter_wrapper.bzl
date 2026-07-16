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

"""Executable wrapper rule for coverage reporter."""

def _reporter_wrapper_impl(ctx):
    launcher = ctx.actions.declare_file(ctx.label.name + ".sh")

    reporter = ctx.executable.reporter
    module_bazel = ctx.file.module_bazel
    coverage_scope = ctx.attr.coverage_scope
    allowlist_group = coverage_scope[OutputGroupInfo].allowlist.to_list()
    objects_group = coverage_scope[OutputGroupInfo].objects.to_list()
    object_files = coverage_scope[OutputGroupInfo].object_files

    if len(allowlist_group) != 1:
        fail("coverage_scope must provide exactly one allowlist file")
    if len(objects_group) != 1:
        fail("coverage_scope must provide exactly one objects manifest file")

    allowlist = allowlist_group[0]
    baseline_objects = objects_group[0]

    script = """#!/usr/bin/env bash
set -euo pipefail
if [[ -z "${{RUNFILES_DIR:-}}" ]]; then
  if [[ -d "$0.runfiles" ]]; then
    export RUNFILES_DIR="$0.runfiles"
  fi
fi
WORKSPACE_ROOT="$(cd "$(dirname "$(readlink -f "${{RUNFILES_DIR}}/{module_bazel}")")" && pwd)/"
exec "${{RUNFILES_DIR}}/{reporter}" \\
  --coverage_allowlist="{allowlist}" \\
  --baseline_objects="{baseline_objects}" \\
  --workspace_root="${{WORKSPACE_ROOT}}" \\
  "$@"
""".format(
        module_bazel = "_main/" + module_bazel.short_path,
        reporter = "_main/" + reporter.short_path,
        allowlist = "_main/" + allowlist.short_path,
        baseline_objects = "_main/" + baseline_objects.short_path,
    )

    ctx.actions.write(
        output = launcher,
        content = script,
        is_executable = True,
    )

    runfiles = ctx.runfiles(
        files = [reporter, allowlist, baseline_objects, module_bazel],
        transitive_files = object_files,
    ).merge(ctx.attr.reporter[DefaultInfo].default_runfiles)

    return [DefaultInfo(
        executable = launcher,
        runfiles = runfiles,
    )]

reporter_wrapper = rule(
    implementation = _reporter_wrapper_impl,
    executable = True,
    attrs = {
        "reporter": attr.label(
            executable = True,
            cfg = "exec",
        ),
        "coverage_scope": attr.label(
            cfg = "target",
        ),
        "module_bazel": attr.label(
            allow_single_file = True,
        ),
    },
)
